#include "hci.h"
#include "backend.h"
#include "driver.h"
#include "hci_proto.h"
#include "internals.h"
#include "logging.h"

#include <errno.h>

BteHciDev _bte_hci_dev;

BteHciEventHandler *_bte_hci_dev_handler_for_event(uint8_t event_code)
{
    if (event_code == HCI_VENDOR_SPECIFIC_EVENT) event_code = 0;
    if (UNLIKELY(event_code > BTE_HCI_EVENT_LAST)) {
        return NULL;
    }
    return &_bte_hci_dev.event_handlers[event_code];
}

bool _bte_hci_dev_foreach_hci_client(BteHciForeachHciClientCb callback,
                                     void *cb_data)
{
    BteHciDev *dev = &_bte_hci_dev;
    for (int i = 0; i < BTE_HCI_MAX_CLIENTS; i++) {
        if (!dev->clients[i]) continue;
        if (callback(&dev->clients[i]->hci, cb_data)) return true;
    }
    return false;
}

static uint16_t build_opcode(uint16_t ocf, uint8_t ogf)
{
    uint16_t opcode_h = (ocf & 0x3ff) | (ogf << 10);
    return htole16(opcode_h);
}

static BteBuffer *hci_command_alloc(uint16_t ocf, uint8_t ogf, uint8_t len)
{
    BteBuffer *b = bte_buffer_alloc_contiguous(len);
    uint8_t *ptr = b->data;
    *(uint16_t*)ptr = build_opcode(ocf, ogf);
    ptr[2] = len - HCI_CMD_HDR_LEN;
    return b;
}

static inline uint16_t hci_command_opcode(BteBuffer *buffer)
{
    return *(uint16_t *)buffer->data;
}

static void hci_dev_dispose(BteHciDev *dev, BteHci *hci)
{
    /* Remove all registered handlers for this client */
    for (int i = 0; i < BTE_HCI_MAX_PENDING_COMMANDS; i++) {
        BteHciPendingCommand *pc = &dev->pending_commands[i];
        if (!bte_data_matcher_is_empty(&pc->matcher) && pc->hci == hci) {
            _bte_hci_dev_free_command(pc);
        }
    }
}

BteHciPendingCommand *_bte_hci_dev_find_pending_command_raw(
    const void *data, size_t len)
{
    BteHciDev *dev = &_bte_hci_dev;

    for (int i = 0; i < BTE_HCI_MAX_PENDING_COMMANDS; i++) {
        BteHciPendingCommand *pc = &dev->pending_commands[i];
        if (!bte_data_matcher_is_empty(&pc->matcher) &&
            bte_data_matcher_compare(&pc->matcher, data, len)) {
            return pc;
        }
    }
    return NULL;
}

BteHciPendingCommand *_bte_hci_dev_find_pending_command(
    const BteBuffer *buffer)
{
    return _bte_hci_dev_find_pending_command_raw(buffer->data, buffer->size);
}

static void deliver_status_to_client(BteBuffer *buffer)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (LIKELY(pc)) {
        /* Free the pending command, but before doing it save the data that we
         * are still using. */
        BteHci *hci = pc->hci;
        BteHciCommandStatusCb command_status_cb =
            pc->command_cb.cmd_status.status;
        BteHciDoneCb client_cb = pc->command_cb.cmd_status.client_cb;

        uint8_t status = buffer->data[HCI_CMD_STATUS_POS_STATUS];
        if (command_status_cb) {
            command_status_cb(hci, status, pc);
        } else {
            _bte_hci_dev_free_command(pc);
        }

        BteHciReply reply;
        reply.status = status;
        client_cb(hci, &reply, hci_userdata(hci));
    }
}

static void deliver_reply_to_client(BteBuffer *buffer)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (LIKELY(pc)) {
        BteHciCommandCb command_cb = pc->command_cb.cmd_complete.complete;
        void *client_cb = pc->command_cb.cmd_complete.client_cb;
        _bte_hci_dev_free_command(pc);

        command_cb(pc->hci, buffer, client_cb);
    }
}

static void handle_info_param(uint16_t ocf, const uint8_t *data, uint8_t len)
{
    BteHciDev *dev = &_bte_hci_dev;
    switch (ocf) {
    case HCI_R_LOC_FEAT_OCF:
        if (data[0] == HCI_SUCCESS) {
            dev->supported_features = read_le64(data + 1);
        } else {
            dev->supported_features = 0;
        }
        dev->info_flags |= BTE_HCI_INFO_GOT_FEATURES;
        break;
    }
}

static void handle_host_control(uint16_t ocf, const uint8_t *data, uint8_t len)
{
    switch (ocf) {
    case HCI_RESET_OCF:
        break;
    }
}

int _bte_hci_send_command(BteBuffer *buffer)
{
    if (UNLIKELY(!buffer)) return -ENOMEM;
    int rc = _bte_backend.hci_send_command(buffer);
    /* The backend, if needed, will increase the reference count. But we
     * ourselves don't need this buffer anymore */
    bte_buffer_unref(buffer);
    return rc;
}

int _bte_hci_send_data(BteBuffer *buffer)
{
    int rc = _bte_backend.hci_send_data(buffer);
    /* The backend, if needed, will increase the reference count. But we
     * ourselves don't need this buffer anymore */
    bte_buffer_unref(buffer);
    return rc;
}

int _bte_hci_dev_handle_event(BteBuffer *buf)
{
    {
        int len = buf->size;
        if (len > 16) len = 16;

        BTE_DEBUG("Event, size %d:", buf->size);
        for (int i = 0; i < len; i++) {
            BTE_DEBUG(" %02x", buf->data[i]);
        }
        BTE_DEBUG("\n");
    }

    uint8_t code = buf->data[0];
    uint8_t len = buf->data[1];
    uint8_t *data = buf->data + 2;
    uint16_t opcode;
    switch (code) {
    case HCI_COMMAND_COMPLETE:
        _bte_hci_dev.num_packets = data[0];
        if (len < 3) break;
        opcode = *(uint16_t *)(data + 1);
        uint16_t opcode_h = le16toh(opcode);
        uint16_t ocf = opcode_h & 0x03ff;
        uint8_t ogf = opcode_h >> 10;
        BTE_DEBUG("opcode %04x, ogf %02x, ocf %04x\n", opcode, ogf, ocf);
        switch (ogf) {
        case HCI_INFO_PARAM_OGF:
            handle_info_param(ocf, data + 3, len - 3);
            break;
        case HCI_HC_BB_OGF:
            handle_host_control(ocf, data + 3, len - 3);
            break;
        }
        deliver_reply_to_client(buf);
        break;
    case HCI_COMMAND_STATUS:
        _bte_hci_dev.num_packets = data[1];
        deliver_status_to_client(buf);
        break;
    }

    BteHciEventHandler *handler = _bte_hci_dev_handler_for_event(code);
    if (handler && handler->handler_cb) {
        handler->handler_cb(buf, handler->cb_data);
    }

    /* The event buffer is unreferenced by the platform backend */
    return 0;
}

int _bte_hci_dev_handle_data(BteBuffer *buf)
{
    int len = buf->size;
    if (len > 10) len = 10;

    BTE_DEBUG("Data, size %d:", buf->size);
    for (int i = 0; i < len; i++) {
        BTE_DEBUG(" %02x", buf->data[i]);
    }
    BTE_DEBUG("\n");
    return 0;
}

int _bte_hci_dev_init()
{
    BteHciDev *dev = &_bte_hci_dev;

    if (dev->init_status != BTE_HCI_INIT_STATUS_UNINITIALIZED) {
        /* Initialization has already started */
        return dev->init_status == BTE_HCI_INIT_STATUS_FAILED ? -EIO : 0;
    }

    dev->init_status = BTE_HCI_INIT_STATUS_INITIALIZING;

    int rc = _bte_backend.init();
    if (rc < 0) return rc;

    return _bte_driver.init(dev);
}

bool _bte_hci_dev_add_client(BteClient *client)
{
    BteHciDev *dev = &_bte_hci_dev;
    for (int i = 0; i < BTE_HCI_MAX_CLIENTS; i++) {
        if (!dev->clients[i]) {
            dev->clients[i] = client;
            return true;
        }
    }
    return false;
}

void _bte_hci_dev_remove_client(BteClient *client)
{
    BteHciDev *dev = &_bte_hci_dev;

    hci_dev_dispose(dev, &client->hci);

    for (int i = 0; i < BTE_HCI_MAX_CLIENTS; i++) {
        if (dev->clients[i] == client) {
            dev->clients[i] = NULL;
            break;
        }
    }
}

void _bte_hci_dev_set_status(BteHciInitStatus status)
{
    BteHciDev *dev = &_bte_hci_dev;
    dev->init_status = status;

    BTE_DEBUG("%s, status %d\n", __func__, status);
    if (status == BTE_HCI_INIT_STATUS_INITIALIZED ||
        status == BTE_HCI_INIT_STATUS_FAILED) {
        bool success = status == BTE_HCI_INIT_STATUS_INITIALIZED;
        for (int i = 0; i < BTE_HCI_MAX_CLIENTS; i++) {
            BteClient *client = dev->clients[i];
            if (client && client->hci.initialized_cb) {
                client->hci.initialized_cb(&client->hci, success,
                                           client->userdata);
                break;
            }
        }
    }
}

BteHciPendingCommand *_bte_hci_dev_alloc_command(const BteDataMatcher *matcher)
{
    BteHciDev *dev = &_bte_hci_dev;

    BteHciPendingCommand *pending_command = NULL;
    if (dev->num_pending_commands == 0) {
        /* Fast track, no checks needed */
        pending_command = &dev->pending_commands[0];
    } else if (dev->num_pending_commands < BTE_HCI_MAX_PENDING_COMMANDS) {
        for (int i = 0; i < BTE_HCI_MAX_PENDING_COMMANDS; i++) {
            BteHciPendingCommand *pc = &dev->pending_commands[i];
            if (bte_data_matcher_is_empty(&pc->matcher)) {
                /* Found a free slot */
                if (!pending_command) {
                    pending_command = pc;
                    break;
                }
            } else if (bte_data_matcher_is_same(matcher, &pc->matcher)) {
                /* The same command has been queued; unless we do some deeper
                 * checks on the buffer data in the reply handler, we won't be
                 * able to match the reply with the pending command, therefore
                 * we refuse it. */
                return NULL;
            }
        }
    }

    if (LIKELY(pending_command)) {
        bte_data_matcher_copy(&pending_command->matcher, matcher);
        dev->num_pending_commands++;
    }

    return pending_command;
}

BteHciPendingCommand *_bte_hci_dev_get_pending_command(
    const BteDataMatcher *matcher)
{
    BteHciDev *dev = &_bte_hci_dev;

    for (int i = 0; i < BTE_HCI_MAX_PENDING_COMMANDS; i++) {
        BteHciPendingCommand *pc = &dev->pending_commands[i];
        if (bte_data_matcher_is_same(matcher, &pc->matcher)) {
            return pc;
        }
    }
    return NULL;
}

BteBuffer *_bte_hci_dev_add_command_no_reply(uint16_t ocf, uint8_t ogf,
                                             uint8_t len)
{
    return hci_command_alloc(ocf, ogf, len);
}

BteBuffer *_bte_hci_dev_add_command(BteHci *hci, uint16_t ocf,
                                    uint8_t ogf, uint8_t len,
                                    uint8_t reply_event,
                                    const BteHciCommandCbUnion *command_cb)
{
    /* We could also take cmd_complete.client_cb, it makes no difference */
    BteHciDoneCb base_cb = command_cb->cmd_status.client_cb;
    BteBuffer *buffer = hci_command_alloc(ocf, ogf, len);
    if (UNLIKELY(!buffer)) goto error_buffer;

    BteDataMatcher matcher;
    bte_data_matcher_init(&matcher);
    bte_data_matcher_add_rule(&matcher, &reply_event, 1, 0);
    if (reply_event == HCI_COMMAND_COMPLETE) {
        bte_data_matcher_add_rule(&matcher, buffer->data, 2,
                                  HCI_CMD_REPLY_POS_OPCODE);
    } else { /* reply_event == HCI_COMMAND_STATUS */
        bte_data_matcher_add_rule(&matcher, buffer->data, 2,
                                  HCI_CMD_STATUS_POS_OPCODE);
    }

    BteHciPendingCommand *pending_command =
        _bte_hci_dev_alloc_command(&matcher);
    if (UNLIKELY(!pending_command)) goto error_command;

    pending_command->command_cb = *command_cb;
    pending_command->hci = hci;
    return buffer;

error_command:
    bte_buffer_unref(buffer);
error_buffer:
    if (base_cb) {
        BteHciReply reply = { HCI_MEMORY_FULL };
        base_cb(hci, &reply, hci_userdata(hci));
    }
    return NULL;
}

void _bte_hci_dev_free_command(BteHciPendingCommand *cmd)
{
    BteHciDev *dev = &_bte_hci_dev;
    bte_data_matcher_init(&cmd->matcher);
    dev->num_pending_commands--;
}

BteBuffer *
_bte_hci_dev_add_pending_command(BteHci *hci, uint16_t ocf,
                                 uint8_t ogf, uint8_t len,
                                 BteHciCommandCb command_cb,
                                 void *client_cb)
{
    BteHciCommandCbUnion cmd = {
        .cmd_complete = { command_cb, client_cb }
    };
    return _bte_hci_dev_add_command(hci, ocf, ogf, len,
                                    HCI_COMMAND_COMPLETE, &cmd);
}

BteBuffer *
_bte_hci_dev_add_pending_async_command(BteHci *hci, uint16_t ocf,
                                       uint8_t ogf, uint8_t len,
                                       BteHciCommandStatusCb command_cb,
                                       void *client_cb)
{
    BteHciCommandCbUnion cmd = {
        .cmd_status = { command_cb, client_cb }
    };
    return _bte_hci_dev_add_command(hci, ocf, ogf, len,
                                    HCI_COMMAND_STATUS, &cmd);
}

void _bte_hci_dev_install_event_handler(uint8_t event_code,
                                        BteHciEventHandlerCb handler_cb,
                                        void *cb_data)
{
    BteHciEventHandler *h = _bte_hci_dev_handler_for_event(event_code);
    if (UNLIKELY(!h)) return;

    if (UNLIKELY(handler_cb && h->handler_cb)) {
        BTE_WARN("Handler already installed for event %02x!", event_code);
    }
    h->handler_cb = handler_cb;
    h->cb_data = cb_data;
}

void _bte_hci_dev_inquiry_cleanup(void)
{
    BteHciDev *dev = &_bte_hci_dev;
    free(dev->inquiry.responses);
    dev->inquiry.responses = NULL;
    dev->inquiry.num_responses = 0;
}

void _bte_hci_dev_stored_keys_cleanup(void)
{
    BteHciDev *dev = &_bte_hci_dev;
    free(dev->stored_keys.responses);
    dev->stored_keys.responses = NULL;
    dev->stored_keys.num_responses = 0;
}
