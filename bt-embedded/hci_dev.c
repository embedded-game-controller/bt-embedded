#include "hci.h"
#include "backend.h"
#include "driver.h"
#include "hci_proto.h"
#include "internals.h"
#include "logging.h"

#include <errno.h>

BteHciDev _bte_hci_dev;

static BteBuffer *hci_command_alloc(uint16_t ocf, uint8_t ogf, uint8_t len)
{
    BteBuffer *b = bte_buffer_alloc_contiguous(len);
    uint8_t *ptr = b->data;
    ptr[0] = ocf & 0xff;
    ptr[1] = (ocf >> 8) | (ogf << 2);
    ptr[2] = len - HCI_CMD_HDR_LEN;
    return b;
}

static inline uint16_t hci_command_opcode(BteBuffer *buffer)
{
    return *(uint16_t *)buffer->data;
}

static inline uint16_t hci_command_reply_opcode(BteBuffer *buffer)
{
    return *(uint16_t *)(buffer->data + 3);
}

static void deliver_reply_to_client(BteBuffer *buffer)
{
    BteHciDev *dev = &_bte_hci_dev;

    BTE_DEBUG("%s, %d pending commands, got opcode %02x \n", __func__,
              dev->num_pending_commands, hci_command_reply_opcode(buffer));
    for (int i = 0; i < BTE_HCI_MAX_PENDING_COMMANDS; i++) {
        BteHciPendingCommand *pc = &dev->pending_commands[i];
        if (pc->buffer) {
            BTE_DEBUG("Found HCI %p with opcode %04x\n", pc->hci,
                      hci_command_opcode(pc->buffer));
        }
        if (pc->buffer &&
            hci_command_opcode(pc->buffer) == hci_command_reply_opcode(buffer)) {
            bte_buffer_unref(pc->buffer);
            pc->buffer = NULL;
            dev->num_pending_commands--;

            pc->command_cb(pc->hci, buffer, pc->client_cb);
            break;
        }
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
    switch (code) {
    case HCI_COMMAND_COMPLETE:
        _bte_hci_dev.num_packets = data[0];
        if (len < 3) break;
        uint16_t opcode = le16toh(*(uint16_t *)(data + 1));
        uint16_t ocf = opcode & 0x03ff;
        uint8_t ogf = opcode >> 10;
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
    }
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

BteBuffer *_bte_hci_dev_add_pending_command(BteHci *hci, uint16_t ocf,
                                            uint8_t ogf, uint8_t len,
                                            BteHciCommandCb command_cb,
                                            void *client_cb)
{
    BteHciDev *dev = &_bte_hci_dev;

    BteHciDoneCb base_cb = client_cb;
    BteBuffer *buffer = hci_command_alloc(ocf, ogf, len);
    if (UNLIKELY(!buffer ||
                 dev->num_pending_commands >= BTE_HCI_MAX_PENDING_COMMANDS)) {
        if (buffer) bte_buffer_unref(buffer);
        BteHciReply reply = { HCI_MEMORY_FULL };
        base_cb(hci, &reply, hci_userdata(hci));
        return NULL;
    }

    BteHciPendingCommand *pending_command = NULL;
    if (dev->num_pending_commands == 0) {
        /* Fast track, no checks needed */
        pending_command = &dev->pending_commands[0];
    } else {
        for (int i = 0; i < BTE_HCI_MAX_PENDING_COMMANDS; i++) {
            BteHciPendingCommand *pc = &dev->pending_commands[i];
            if (!pc->buffer) {
                /* Found a free slot */
                if (!pending_command) pending_command = pc;
            } else if (hci_command_opcode(pc->buffer) ==
                       hci_command_opcode(buffer)) {
                /* The same command has been queued; unless we do some deeper
                 * checks on the buffer data in the reply handler, we won't be
                 * able to match the reply with the pending command, therefore
                 * we refuse it. */
                bte_buffer_unref(buffer);
                BteHciReply reply = { HCI_MEMORY_FULL };
                base_cb(hci, &reply, hci_userdata(hci));
                return NULL;
            }
        }
    }

    if (LIKELY(pending_command)) {
        pending_command->buffer = bte_buffer_ref(buffer);
        pending_command->command_cb = command_cb;
        pending_command->hci = hci;
        pending_command->client_cb = client_cb;
        dev->num_pending_commands++;
        return buffer;
    }

    /* This should never happen, unless num_pending_commands is wrong */
    return NULL;
}
