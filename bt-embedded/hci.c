#include "hci.h"

#include "buffer.h"
#include "internals.h"
#include "logging.h"

static void command_complete_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    if (!client_cb) return;
    BteHciReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    BteHciDoneCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

static void write_clock_offset(uint16_t clock_offset, uint8_t *data)
{
    if (clock_offset != BTE_HCI_CLOCK_OFFSET_INVALID) {
        write_le16(clock_offset & 0x7fff, data);
    } else {
        write_le16(0, data);
    }
}

static void common_read_connection_status_cb(BteHci *hci, uint8_t status,
                                             BteHciPendingCommand *pc)
{
    if (status != 0) goto error;

    struct _bte_hci_tmpdata_common_read_connection_t *tmpdata =
        &hci->last_async_cmd_data.common_read_connection;
    BteDataMatcher matcher;
    bte_data_matcher_init(&matcher);
    bte_data_matcher_add_rule(&matcher, &tmpdata->event_code, 1, 0);
    uint8_t handle_le[2];
    write_le16(tmpdata->conn_handle, &handle_le);
    bte_data_matcher_add_rule(&matcher, handle_le, 2,
                              /* 1 for the status byte */
                              HCI_CMD_EVENT_POS_DATA + 1);
    BteHciPendingCommand *ev = _bte_hci_dev_alloc_command(&matcher);
    if (UNLIKELY(!ev)) goto error;

    ev->hci = hci;
    ev->command_cb.event_common_read_connection.client_cb = tmpdata->client_cb;
    _bte_hci_dev_install_event_handler(
        tmpdata->event_code, tmpdata->handler_cb, NULL);

error:
    _bte_hci_dev_free_command(pc);
}

static void common_read_connection(BteHci *hci,
                                   uint16_t ocf, uint8_t ogf,
                                   BteConnHandle conn_handle,
                                   uint8_t event_code,
                                   BteHciEventHandlerCb event_handler_cb,
                                   BteHciDoneCb status_cb,
                                   void *client_cb)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, ocf, ogf,
        HCI_CMD_HDR_LEN + 2, /* 2 for the connection handle */
        common_read_connection_status_cb, status_cb);
    if (UNLIKELY(!b)) return;

    /* In the status callback we read this and setup the event matcher */
    struct _bte_hci_tmpdata_common_read_connection_t *tmpdata =
        &hci->last_async_cmd_data.common_read_connection;
    tmpdata->conn_handle = conn_handle;
    tmpdata->client_cb = client_cb;
    tmpdata->event_code = event_code;
    tmpdata->handler_cb = event_handler_cb;

    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    write_le16(conn_handle, data);
    _bte_hci_send_command(b);
}

BteHci *bte_hci_get(BteClient *client)
{
    return &client->hci;
}

BteClient *bte_hci_get_client(BteHci *hci)
{
    return hci_client(hci);
}

void bte_hci_on_initialized(BteHci *hci, BteInitializedCb callback)
{
    BteHciDev *dev = &_bte_hci_dev;
    hci->initialized_cb = callback;
    if (dev->init_status == BTE_HCI_INIT_STATUS_INITIALIZED ||
        dev->init_status == BTE_HCI_INIT_STATUS_FAILED) {
        callback(hci, dev->init_status == BTE_HCI_INIT_STATUS_INITIALIZED,
                 hci_userdata(hci));
    }
}

BteHciSupportedFeatures bte_hci_get_supported_features(BteHci *)
{
    return _bte_hci_dev.supported_features;
}

uint16_t bte_hci_get_acl_mtu(BteHci *hci)
{
    return _bte_hci_dev.acl_mtu;
}

uint8_t bte_hci_get_sco_mtu(BteHci *hci)
{
    return _bte_hci_dev.sco_mtu;
}

uint16_t bte_hci_get_acl_max_packets(BteHci *hci)
{
    return _bte_hci_dev.acl_max_packets;
}

uint16_t bte_hci_get_sco_max_packets(BteHci *hci)
{
    return _bte_hci_dev.sco_max_packets;
}

void bte_hci_nop(BteHci *hci, BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, 0, 0, 3,
        command_complete_cb, callback);
    _bte_hci_send_command(b);
}

static void inquiry_result_cb(BteBuffer *buffer, void *cb_data)
{
    BteHciDev *dev = &_bte_hci_dev;

    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_HDR_LEN;
    int num_responses = data[0];
    data++;

    ensure_array_size((void**)&dev->inquiry.responses,
                      sizeof(BteHciInquiryResponse), 32,
                      dev->inquiry.num_responses, num_responses);
    if (UNLIKELY(!dev->inquiry.responses)) return;

    BteHciInquiryResponse *responses = dev->inquiry.responses;
    int i_tail = dev->inquiry.num_responses;
    for (int i = 0; i < num_responses; i++) {
        BteHciInquiryResponse *r = &responses[i_tail];
        uint8_t *ptr = data;
        memcpy(&r->address, ptr + sizeof(r->address) * i, sizeof(r->address));
        ptr += sizeof(r->address) * num_responses;
        r->page_scan_rep_mode = ptr[i];
        ptr += num_responses;
        r->page_scan_period_mode = ptr[i];
        ptr += num_responses;
        r->reserved = ptr[i];
        ptr += num_responses;
        int cod_size = sizeof(r->class_of_device);
        memcpy(&r->class_of_device, ptr + cod_size * i, cod_size);
        ptr += cod_size * num_responses;
        r->clock_offset = le16toh(*((uint16_t*)ptr + i));
        /* Check if the record is a duplicate */
        bool duplicate = false;
        for (int j = 0; j < dev->inquiry.num_responses; j++) {
            if (memcmp(r, &responses[j], sizeof(*r)) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) i_tail++;
    }
    dev->inquiry.num_responses = i_tail;
}

static void inquiry_event_cb(BteBuffer *buffer, void *cb_data)
{
    BteHciDev *dev = &_bte_hci_dev;
    BteHci *hci = cb_data;
    BteHciInquiryReply reply;

    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_HDR_LEN;
    reply.status = data[0];
    reply.num_responses = dev->inquiry.num_responses;
    reply.responses = dev->inquiry.responses;

    BteHciInquiryCb inquiry_cb = hci->inquiry_cb;
    _bte_hci_dev_install_event_handler(HCI_INQUIRY_COMPLETE, NULL, NULL);
    _bte_hci_dev_install_event_handler(HCI_INQUIRY_RESULT, NULL, NULL);
    hci->inquiry_cb = NULL;

    inquiry_cb(hci, &reply, hci_userdata(hci));
    _bte_hci_dev_inquiry_cleanup();
}

static void inquiry_status_cb(BteHci *hci, uint8_t status,
                              BteHciPendingCommand *pc)
{
    if (status == 0) {
        _bte_hci_dev_install_event_handler(HCI_INQUIRY_RESULT,
                                           inquiry_result_cb, hci);
        _bte_hci_dev_install_event_handler(HCI_INQUIRY_COMPLETE,
                                           inquiry_event_cb, hci);
    } else {
        hci->inquiry_cb = NULL;
    }
    _bte_hci_dev_free_command(pc);
}

void bte_hci_inquiry(BteHci *hci, BteLap lap, uint8_t len, uint8_t max_resp,
                     BteHciDoneCb status_cb, BteHciInquiryCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, HCI_INQUIRY_OCF, HCI_LINK_CTRL_OGF, HCI_INQUIRY_PLEN,
        inquiry_status_cb, status_cb);
    if (UNLIKELY(!b)) return;

    hci->inquiry_cb = callback;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    data[0] = lap & 0xff;
    data[1] = (lap >> 8) & 0xff;
    data[2] = (lap >> 16) & 0xff;
    data[3] = len;
    data[4] = max_resp;
    _bte_hci_send_command(b);
}

void bte_hci_inquiry_cancel(BteHci *hci, BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci,
        HCI_INQUIRY_CANCEL_OCF, HCI_LINK_CTRL_OGF, HCI_INQUIRY_CANCEL_PLEN,
        command_complete_cb, callback);
    _bte_hci_send_command(b);
}

static void periodic_inquiry_event_cb(BteBuffer *buffer, void *cb_data)
{
    BteHciDev *dev = &_bte_hci_dev;
    BteHci *hci = cb_data;

    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_HDR_LEN;
    BteHciInquiryReply reply;
    reply.status = data[0];
    reply.num_responses = dev->inquiry.num_responses;
    reply.responses = dev->inquiry.responses;

    hci->inquiry_cb(hci, &reply, hci_userdata(hci));
    _bte_hci_dev_inquiry_cleanup();
}

static void periodic_inquiry_complete_cb(BteHci *hci, BteBuffer *buffer,
                                         void *client_cb)
{
    uint8_t status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    if (status == 0) {
        _bte_hci_dev_install_event_handler(HCI_INQUIRY_RESULT,
                                           inquiry_result_cb, hci);
        _bte_hci_dev_install_event_handler(HCI_INQUIRY_COMPLETE,
                                           periodic_inquiry_event_cb, hci);
    } else {
        hci->inquiry_cb = NULL;
    }
    command_complete_cb(hci, buffer, client_cb);
}

void bte_hci_periodic_inquiry(BteHci *hci,
                              uint16_t min_period, uint16_t max_period,
                              BteLap lap, uint8_t len, uint8_t max_resp,
                              BteHciDoneCb status_cb, BteHciInquiryCb callback)
{
    /* The periodic inquiry command does not sent a command status (like the
     * inquiry command), but a command complete. So we treat it like an
     * ordinary synchronous command. */
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci,
        HCI_PERIODIC_INQUIRY_OCF, HCI_LINK_CTRL_OGF, HCI_PERIODIC_INQUIRY_PLEN,
        periodic_inquiry_complete_cb, status_cb);
    if (UNLIKELY(!b)) return;

    _bte_hci_dev_inquiry_cleanup();
    hci->inquiry_cb = callback;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    *(uint16_t *)&data[0] = htole16(max_period);
    *(uint16_t *)&data[2] = htole16(min_period);
    data[4] = lap & 0xff;
    data[5] = (lap >> 8) & 0xff;
    data[6] = (lap >> 16) & 0xff;
    data[7] = len;
    data[8] = max_resp;
    _bte_hci_send_command(b);
}

static void exit_periodic_inquiry_cb(BteHci *hci, BteBuffer *buffer,
                                     void *client_cb)
{
    _bte_hci_dev_inquiry_cleanup();
    _bte_hci_dev_install_event_handler(HCI_INQUIRY_COMPLETE, NULL, NULL);
    _bte_hci_dev_install_event_handler(HCI_INQUIRY_RESULT, NULL, NULL);
    hci->inquiry_cb = NULL;
    command_complete_cb(hci, buffer, client_cb);
}

void bte_hci_exit_periodic_inquiry(BteHci *hci, BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci,
        HCI_EXIT_PERIODIC_INQUIRY_OCF, HCI_LINK_CTRL_OGF,
        HCI_EXIT_PERIODIC_INQUIRY_PLEN,
        exit_periodic_inquiry_cb, callback);
    _bte_hci_send_command(b);
}

static void conn_complete_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_HDR_LEN;
    BteHciCreateConnectionReply reply;
    reply.status = data[0];
    data++;
    reply.conn_handle = read_le16(data);
    data += 2;
    memcpy(&reply.address, data, 6);
    data += 6;
    reply.link_type = data[0];
    reply.encryption_mode = data[1];

    BteHciCreateConnectionCb create_connection_cb =
        pc->command_cb.event_conn_complete.client_cb;
    _bte_hci_dev_free_command(pc);

    create_connection_cb(hci, &reply, hci_userdata(hci));
}

static void create_connection_status_cb(BteHci *hci, uint8_t status,
                                        BteHciPendingCommand *pc)
{
    if (status != 0) goto error;

    struct _bte_hci_tmpdata_create_connection_t *tmpdata =
        &hci->last_async_cmd_data.create_connection;
    BteDataMatcher matcher;
    bte_data_matcher_init(&matcher);
    uint8_t event_type = HCI_CONNECTION_COMPLETE;
    bte_data_matcher_add_rule(&matcher, &event_type, 1, 0);
    bte_data_matcher_add_rule(&matcher,
                              &tmpdata->address, 6,
                              /* 3 = 1 status byte + 2 conn handle */
                              HCI_CMD_REPLY_POS_HDR_LEN + 3);
    BteHciPendingCommand *ev = _bte_hci_dev_alloc_command(&matcher);
    if (UNLIKELY(!ev)) goto error;

    ev->hci = hci;
    ev->command_cb.event_conn_complete.client_cb = tmpdata->client_cb;
    _bte_hci_dev_install_event_handler(HCI_CONNECTION_COMPLETE,
                                       conn_complete_event_cb, NULL);

error:
    _bte_hci_dev_free_command(pc);
}

void bte_hci_create_connection(BteHci *hci,
                               const BteBdAddr *address,
                               BtePacketType packet_type,
                               uint8_t page_scan_rep_mode,
                               uint16_t clock_offset,
                               bool allow_role_switch,
                               BteHciDoneCb status_cb,
                               BteHciCreateConnectionCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, HCI_CREATE_CONN_OCF, HCI_LINK_CTRL_OGF, HCI_CREATE_CONN_PLEN,
        create_connection_status_cb, status_cb);
    if (UNLIKELY(!b)) return;

    /* In the status callback we read this and setup the event matcher */
    struct _bte_hci_tmpdata_create_connection_t *tmpdata =
        &hci->last_async_cmd_data.create_connection;
    memcpy(&tmpdata->address, address, sizeof(*address));
    tmpdata->client_cb = callback;

    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    memcpy(data, address, sizeof(*address));
    data += sizeof(*address);
    write_le16(packet_type, data);
    data += 2;
    data[0] = page_scan_rep_mode;
    data++;
    data[0] = 0; /* reserved */
    data++;
    write_clock_offset(clock_offset, data);
    data += 2;
    data[0] = allow_role_switch;
    _bte_hci_send_command(b);
}

static void disconnect_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    /* TODO: will need to propagate this to the higher layers */
    command_complete_cb(hci, buffer, client_cb);
}

void bte_hci_disconnect(BteHci *hci, BteConnHandle handle, uint8_t reason,
                        BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_DISCONN_OCF, HCI_LINK_CTRL_OGF, HCI_DISCONN_PLEN,
        disconnect_cb, callback);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    write_le16(handle, data);
    data[2] = reason;
    _bte_hci_send_command(b);
}

void bte_hci_create_connection_cancel(BteHci *hci, const BteBdAddr *address,
                                      BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_CREATE_CONN_CANCEL_OCF, HCI_LINK_CTRL_OGF,
        HCI_CREATE_CONN_CANCEL_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    memcpy(b->data + HCI_CMD_HDR_LEN, address, sizeof(*address));
    _bte_hci_send_command(b);
}

void bte_hci_accept_connection(BteHci *hci,
                               const BteBdAddr *address, uint8_t role,
                               BteHciDoneCb status_cb,
                               BteHciAcceptConnectionCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, HCI_ACCEPT_CONN_REQ_OCF, HCI_LINK_CTRL_OGF,
        HCI_ACCEPT_CONN_REQ_PLEN,
        /* The status CB for the create connection fits us as well */
        create_connection_status_cb, status_cb);
    if (UNLIKELY(!b)) return;

    /* In the status callback we read this and setup the event matcher */
    struct _bte_hci_tmpdata_create_connection_t *tmpdata =
        &hci->last_async_cmd_data.create_connection;
    memcpy(&tmpdata->address, address, sizeof(*address));
    tmpdata->client_cb = callback;

    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    memcpy(data, address, sizeof(*address));
    data += sizeof(*address);
    data[0] = role;
    _bte_hci_send_command(b);
}

void bte_hci_reject_connection(BteHci *hci,
                               const BteBdAddr *address, uint8_t reason,
                               BteHciDoneCb status_cb,
                               BteHciRejectConnectionCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, HCI_REJECT_CONN_REQ_OCF, HCI_LINK_CTRL_OGF,
        HCI_REJECT_CONN_REQ_PLEN,
        /* The status CB for the create connection fits us as well */
        create_connection_status_cb, status_cb);
    if (UNLIKELY(!b)) return;

    /* In the status callback we read this and setup the event matcher */
    struct _bte_hci_tmpdata_create_connection_t *tmpdata =
        &hci->last_async_cmd_data.create_connection;
    memcpy(&tmpdata->address, address, sizeof(*address));
    tmpdata->client_cb = callback;

    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    memcpy(data, address, sizeof(*address));
    data += sizeof(*address);
    data[0] = reason;
    _bte_hci_send_command(b);
}

static bool client_handle_connection_request(BteHci *hci, void *cb_data)
{
    const uint8_t *data = cb_data;
    const BteBdAddr *address = (void*)data;
    const BteClassOfDevice *cod = (void*)(data + 6);
    uint8_t link_type = data[6 + 3];
    return hci->connection_request_cb &&
        hci->connection_request_cb(hci, address, cod, link_type,
                                   hci_userdata(hci));
}

static void connection_request_event_cb(BteBuffer *buffer, void *cb_data)
{
    uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    _bte_hci_dev_foreach_hci_client(client_handle_connection_request, data);
}

void bte_hci_on_connection_request(BteHci *hci, BteHciConnectionRequestCb callback)
{
    hci->connection_request_cb = callback;
    _bte_hci_dev_install_event_handler(HCI_CONNECTION_REQUEST,
                                       connection_request_event_cb, NULL);
}

static bool client_handle_link_key_request(BteHci *hci, void *cb_data)
{
    const BteBdAddr *address = cb_data;
    return hci->link_key_request_cb &&
        hci->link_key_request_cb(hci, address, hci_userdata(hci));
}

static void link_key_request_event_cb(BteBuffer *buffer, void *cb_data)
{
    uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    _bte_hci_dev_foreach_hci_client(client_handle_link_key_request, data);
}

void bte_hci_on_link_key_request(BteHci *hci, BteHciLinkKeyRequestCb callback)
{
    hci->link_key_request_cb = callback;
    _bte_hci_dev_install_event_handler(HCI_LINK_KEY_REQUEST,
                                       link_key_request_event_cb, NULL);
}

static void link_key_req_reply_cb(BteHci *hci, BteBuffer *buffer,
                                  void *client_cb)
{
    if (!client_cb) return;
    BteHciLinkKeyReqReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    memcpy(&reply.address, buffer->data + HCI_CMD_REPLY_POS_DATA,
           sizeof(reply.address));
    BteHciLinkKeyReqReplyCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_link_key_req_reply(BteHci *hci, const BteBdAddr *address,
                                const BteLinkKey *key,
                                BteHciLinkKeyReqReplyCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_LINK_KEY_REQ_REP_OCF, HCI_LINK_CTRL_OGF,
        HCI_LINK_KEY_REQ_REP_PLEN,
        link_key_req_reply_cb, callback);
    if (UNLIKELY(!b)) return;
    memcpy(b->data + HCI_CMD_HDR_LEN, address, sizeof(*address));
    memcpy(b->data + HCI_CMD_HDR_LEN + sizeof(*address), key, sizeof(*key));
    _bte_hci_send_command(b);
}

void bte_hci_link_key_req_neg_reply(BteHci *hci, const BteBdAddr *address,
                                    BteHciLinkKeyReqReplyCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_LINK_KEY_REQ_NEG_REP_OCF, HCI_LINK_CTRL_OGF,
        HCI_LINK_KEY_REQ_NEG_REP_PLEN,
        link_key_req_reply_cb, callback);
    if (UNLIKELY(!b)) return;
    memcpy(b->data + HCI_CMD_HDR_LEN, address, sizeof(*address));
    _bte_hci_send_command(b);
}

static bool client_handle_pin_code_request(BteHci *hci, void *cb_data)
{
    const BteBdAddr *address = cb_data;
    return hci->pin_code_request_cb &&
        hci->pin_code_request_cb(hci, address, hci_userdata(hci));
}

static void pin_code_request_event_cb(BteBuffer *buffer, void *cb_data)
{
    uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    _bte_hci_dev_foreach_hci_client(client_handle_pin_code_request, data);
}

void bte_hci_on_pin_code_request(BteHci *hci, BteHciPinCodeRequestCb callback)
{
    hci->pin_code_request_cb = callback;
    _bte_hci_dev_install_event_handler(HCI_PIN_CODE_REQUEST,
                                       pin_code_request_event_cb, NULL);
}

void bte_hci_pin_code_req_reply(BteHci *hci, const BteBdAddr *address,
                                const uint8_t *pin, uint8_t len,
                                BteHciPinCodeReqReplyCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_PIN_CODE_REQ_REP_OCF, HCI_LINK_CTRL_OGF,
        HCI_PIN_CODE_REQ_REP_PLEN,
        /* Reuse the callback for the link key, since the code is the same */
        link_key_req_reply_cb, callback);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    memcpy(data, address, sizeof(*address));
    data += sizeof(*address);
    data[0] = len; data++;
    memcpy(data, pin, len);
    data[len] = 0; /* Just to be on the safe side */
    _bte_hci_send_command(b);
}

void bte_hci_pin_code_req_neg_reply(BteHci *hci, const BteBdAddr *address,
                                    BteHciPinCodeReqReplyCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_PIN_CODE_REQ_NEG_REP_OCF, HCI_LINK_CTRL_OGF,
        HCI_PIN_CODE_REQ_NEG_REP_PLEN,
        link_key_req_reply_cb, callback);
    if (UNLIKELY(!b)) return;
    memcpy(b->data + HCI_CMD_HDR_LEN, address, sizeof(*address));
    _bte_hci_send_command(b);
}

static void auth_complete_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    BteHciAuthRequestedReply reply;
    reply.status = data[0];
    reply.conn_handle = read_le16(data + 1);

    BteHciAuthRequestedCb auth_requested_cb =
        pc->command_cb.event_auth_complete.client_cb;
    _bte_hci_dev_free_command(pc);

    auth_requested_cb(hci, &reply, hci_userdata(hci));
}

void bte_hci_auth_requested(BteHci *hci,
                            BteConnHandle conn_handle,
                            BteHciDoneCb status_cb,
                            BteHciAuthRequestedCb callback)
{
    common_read_connection(hci, HCI_AUTH_REQUESTED_OCF, HCI_LINK_CTRL_OGF,
                           conn_handle,
                           HCI_AUTH_COMPLETE, auth_complete_event_cb,
                           status_cb, callback);
}

static void remote_name_req_complete_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    const uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    BteHciReadRemoteNameReply reply;
    reply.status = data[0];
    memcpy(&reply.address, data + 1, 6);
    strncpy(reply.name, (const void*)(data + 1 + 6), 248);
    reply.name[248] = '\0';

    BteHciReadRemoteNameCb read_remote_name_cb =
        pc->command_cb.event_remote_name_req_complete.client_cb;
    _bte_hci_dev_free_command(pc);

    read_remote_name_cb(hci, &reply, hci_userdata(hci));
}

static void read_remote_name_status_cb(BteHci *hci, uint8_t status,
                                       BteHciPendingCommand *pc)
{
    if (status != 0) goto error;

    struct _bte_hci_tmpdata_read_remote_name_t *tmpdata =
        &hci->last_async_cmd_data.read_remote_name;
    BteDataMatcher matcher;
    bte_data_matcher_init(&matcher);
    uint8_t event_type = HCI_REMOTE_NAME_REQ_COMPLETE;
    bte_data_matcher_add_rule(&matcher, &event_type, 1, 0);
    bte_data_matcher_add_rule(&matcher, &tmpdata->address, 6,
                              /* 1 for the status byte */
                              HCI_CMD_EVENT_POS_DATA + 1);
    BteHciPendingCommand *ev = _bte_hci_dev_alloc_command(&matcher);
    if (UNLIKELY(!ev)) goto error;

    ev->hci = hci;
    ev->command_cb.event_remote_name_req_complete.client_cb =
        tmpdata->client_cb;
    _bte_hci_dev_install_event_handler(
        HCI_REMOTE_NAME_REQ_COMPLETE, remote_name_req_complete_event_cb, NULL);

error:
    _bte_hci_dev_free_command(pc);
}

void bte_hci_read_remote_name(BteHci *hci,
                              const BteBdAddr *address,
                              uint8_t page_scan_rep_mode,
                              uint16_t clock_offset,
                              BteHciDoneCb status_cb,
                              BteHciReadRemoteNameCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, HCI_R_REMOTE_NAME_OCF, HCI_LINK_CTRL_OGF,
        HCI_R_REMOTE_NAME_PLEN,
        read_remote_name_status_cb, status_cb);
    if (UNLIKELY(!b)) return;

    /* In the status callback we read this and setup the event matcher */
    struct _bte_hci_tmpdata_read_remote_name_t *tmpdata =
        &hci->last_async_cmd_data.read_remote_name;
    memcpy(&tmpdata->address, address, sizeof(*address));
    tmpdata->client_cb = callback;

    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    memcpy(data, address, sizeof(*address));
    data += sizeof(*address);
    data[0] = page_scan_rep_mode;
    data++;
    data[0] = 0; /* reserved */
    data++;
    write_clock_offset(clock_offset, data);
    _bte_hci_send_command(b);
}

static void read_remote_features_complete_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    const uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    BteHciReadRemoteFeaturesReply reply;
    reply.status = data[0];
    reply.conn_handle = read_le16(data + 1);
    reply.features = read_le64(data + 3);

    BteHciReadRemoteFeaturesCb read_remote_features_cb =
        pc->command_cb.event_read_remote_features_complete.client_cb;
    _bte_hci_dev_free_command(pc);

    read_remote_features_cb(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_remote_features(BteHci *hci, BteConnHandle conn_handle,
                                  BteHciDoneCb status_cb,
                                  BteHciReadRemoteFeaturesCb callback)
{
    common_read_connection(
        hci, HCI_R_REMOTE_FEATURES_OCF, HCI_LINK_CTRL_OGF, conn_handle,
        HCI_READ_REMOTE_FEATURES_COMPLETE,
        read_remote_features_complete_event_cb,
        status_cb, callback);
}

static void read_remote_version_info_complete_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    const uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    BteHciReadRemoteVersionInfoReply reply;
    reply.status = data[0];
    data++;
    reply.conn_handle = read_le16(data);
    data += 2;
    reply.lmp_version = data[0];
    data++;
    reply.manufacturer_name = read_le16(data);
    data += 2;
    reply.lmp_subversion = read_le16(data);

    BteHciReadRemoteVersionInfoCb read_remote_version_info_cb =
        pc->command_cb.event_read_remote_version_info_complete.client_cb;
    _bte_hci_dev_free_command(pc);

    read_remote_version_info_cb(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_remote_version_info(BteHci *hci,
                                      BteConnHandle conn_handle,
                                      BteHciDoneCb status_cb,
                                      BteHciReadRemoteVersionInfoCb callback)
{
    common_read_connection(
        hci, HCI_R_REMOTE_VERSION_INFO_OCF, HCI_LINK_CTRL_OGF, conn_handle,
        HCI_READ_REMOTE_VERSION_COMPLETE,
        read_remote_version_info_complete_event_cb,
        status_cb, callback);
}

static void read_clock_offset_complete_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    const uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    BteHciReadClockOffsetReply reply;
    reply.status = data[0];
    reply.conn_handle = read_le16(data + 1);
    reply.clock_offset = read_le16(data + 3);

    BteHciReadClockOffsetCb read_clock_offset_cb =
        pc->command_cb.event_read_clock_offset_complete.client_cb;
    _bte_hci_dev_free_command(pc);

    read_clock_offset_cb(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_clock_offset(BteHci *hci,
                               BteConnHandle conn_handle,
                               BteHciDoneCb status_cb,
                               BteHciReadClockOffsetCb callback)
{
    common_read_connection(
        hci, HCI_R_CLOCK_OFFSET_OCF, HCI_LINK_CTRL_OGF, conn_handle,
        HCI_READ_CLOCK_OFFSET_COMPLETE, read_clock_offset_complete_event_cb,
        status_cb, callback);
}

void bte_hci_set_sniff_mode(BteHci *hci, BteConnHandle conn_handle,
                            uint16_t min_interval, uint16_t max_interval,
                            uint16_t attempt_slots, uint16_t timeout,
                            BteHciDoneCb status_cb)
{
    BteBuffer *b = _bte_hci_dev_add_pending_async_command(
        hci, HCI_SNIFF_MODE_OCF, HCI_LINK_POLICY_OGF, HCI_SNIFF_MODE_PLEN,
        NULL, status_cb);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    write_le16(conn_handle, data);
    data += 2;
    write_le16(max_interval, data);
    data += 2;
    write_le16(min_interval, data);
    data += 2;
    write_le16(attempt_slots, data);
    data += 2;
    write_le16(timeout, data);
    _bte_hci_send_command(b);
}

static void mode_change_event_cb(BteBuffer *buffer, void *)
{
    BteHciPendingCommand *pc = _bte_hci_dev_find_pending_command(buffer);
    if (UNLIKELY(!pc)) return;

    BteHci *hci = pc->hci;
    uint8_t *data = buffer->data + HCI_CMD_EVENT_POS_DATA;
    BteHciModeChangeReply reply;
    reply.status = data[0];
    data++;
    reply.conn_handle = read_le16(data);
    data += 2;
    reply.current_mode = data[0];
    data++;
    reply.interval = read_le16(data);

    BteHciModeChangeCb cb = pc->command_cb.event_mode_change.client_cb;
    if (cb(hci, &reply, hci_userdata(hci))) {
        _bte_hci_dev_free_command(pc);
    }
}

void bte_hci_on_mode_change(BteHci *hci, BteConnHandle conn_handle,
                            BteHciModeChangeCb callback)
{
    BteDataMatcher matcher;
    bte_data_matcher_init(&matcher);
    uint8_t event_type = HCI_MODE_CHANGE;
    bte_data_matcher_add_rule(&matcher, &event_type, 1, 0);
    uint8_t handle_le[2];
    write_le16(conn_handle, &handle_le);
    bte_data_matcher_add_rule(&matcher, handle_le, 2,
                              /* 1 for the status byte */
                              HCI_CMD_EVENT_POS_DATA + 1);
    if (!callback) {
        /* We must remove our listener */
        BteHciPendingCommand *ev = _bte_hci_dev_get_pending_command(&matcher);
        if (ev && ev->hci == hci) _bte_hci_dev_free_command(ev);
        return;
    }
    BteHciPendingCommand *ev = _bte_hci_dev_alloc_command(&matcher);
    if (UNLIKELY(!ev)) return; /* already installed */

    ev->hci = hci;
    ev->command_cb.event_mode_change.client_cb = callback;
    _bte_hci_dev_install_event_handler(HCI_MODE_CHANGE,
                                       mode_change_event_cb, NULL);
}

static void read_link_policy_settings_cb(BteHci *hci, BteBuffer *buffer,
                                         void *client_cb)
{
    BteHciReadLinkPolicySettingsReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    const uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    reply.conn_handle = read_le16(data);
    reply.settings = read_le16(data + 2);
    BteHciReadLinkPolicySettingsCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_link_policy_settings(BteHci *hci,
                                       BteConnHandle conn_handle,
                                       BteHciReadLinkPolicySettingsCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_LINK_POLICY_OCF, HCI_LINK_POLICY_OGF, HCI_R_LINK_POLICY_PLEN,
        read_link_policy_settings_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(conn_handle, b->data + HCI_CMD_HDR_LEN);
    _bte_hci_send_command(b);
}

void bte_hci_write_link_policy_settings(BteHci *hci,
                                        BteConnHandle conn_handle,
                                        BteHciLinkPolicySettings settings,
                                        BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_LINK_POLICY_OCF, HCI_LINK_POLICY_OGF,
        HCI_W_LINK_POLICY_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(conn_handle, b->data + HCI_CMD_HDR_LEN);
    write_le16(settings, b->data + HCI_CMD_HDR_LEN + 2);
    _bte_hci_send_command(b);
}

void bte_hci_set_event_mask(BteHci *hci, BteHciEventMask mask,
                            BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_SET_EV_MASK_OCF, HCI_HC_BB_OGF, HCI_SET_EV_MASK_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    uint64_t le_mask = htole64(mask);
    memcpy(b->data + HCI_CMD_HDR_LEN, &le_mask, sizeof(le_mask));
    _bte_hci_send_command(b);
}

void bte_hci_reset(BteHci *hci, BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_RESET_OCF, HCI_HC_BB_OGF, HCI_RESET_PLEN,
        command_complete_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_set_event_filter(BteHci *hci, uint8_t filter_type,
                              uint8_t cond_type, const void *filter_data,
                              BteHciDoneCb callback)
{
    uint8_t cond_len = 0;
    switch (filter_type) {
    case BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT:
        switch (cond_type) {
        case BTE_HCI_COND_TYPE_INQUIRY_COD: cond_len = 6; break;
        case BTE_HCI_COND_TYPE_INQUIRY_ADDRESS: cond_len = 6; break;
        }
        break;
    case BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP:
        switch (cond_type) {
        case BTE_HCI_COND_TYPE_CONN_SETUP_ALL: cond_len = 1; break;
        case BTE_HCI_COND_TYPE_CONN_SETUP_COD: cond_len = 7; break;
        case BTE_HCI_COND_TYPE_CONN_SETUP_ADDRESS: cond_len = 7; break;
        }
        break;
    }
    uint8_t filter_len = 0;
    if (filter_type != BTE_HCI_EVENT_FILTER_TYPE_CLEAR) {
        filter_len = 1 + cond_len;
    }

    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_SET_EV_FILTER_OCF, HCI_HC_BB_OGF,
        HCI_SET_EV_FILTER_PLEN + filter_len,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    data[0] = filter_type;
    if (filter_len > 0) {
        data[1] = cond_type;
        if (cond_len > 0) {
            memcpy(data + 2, filter_data, cond_len);
        }
    }
    _bte_hci_send_command(b);
}

static void read_pin_type_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    BteHciReadPinTypeReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.pin_type = buffer->data[HCI_CMD_REPLY_POS_DATA];
    BteHciReadPinTypeCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_pin_type(BteHci *hci, BteHciReadPinTypeCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_PIN_TYPE_OCF, HCI_HC_BB_OGF, HCI_R_PIN_TYPE_PLEN,
        read_pin_type_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_pin_type(BteHci *hci, uint8_t pin_type,
                            BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_PIN_TYPE_OCF, HCI_HC_BB_OGF, HCI_W_PIN_TYPE_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = pin_type;
    _bte_hci_send_command(b);
}

static void return_link_keys_cb(BteBuffer *buffer, void *cb_data)
{
    BteHciDev *dev = &_bte_hci_dev;

    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_HDR_LEN;
    int num_responses = data[0];
    data++;

    ensure_array_size((void**)&dev->stored_keys.responses,
                      sizeof(BteHciStoredLinkKey), 16,
                      dev->stored_keys.num_responses, num_responses);
    if (UNLIKELY(!dev->stored_keys.responses)) return;

    BteHciStoredLinkKey *responses = dev->stored_keys.responses;
    int i_tail = dev->stored_keys.num_responses;
    for (int i = 0; i < num_responses; i++) {
        BteHciStoredLinkKey *r = &responses[i_tail + i];
        uint8_t *ptr = data;
        memcpy(&r->address, ptr + sizeof(r->address) * i, sizeof(r->address));
        ptr += sizeof(r->address) * num_responses;
        memcpy(&r->key, ptr + sizeof(r->key) * i, sizeof(r->key));
    }
    dev->stored_keys.num_responses += num_responses;
}

static void read_stored_link_key_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    BteHciDev *dev = &_bte_hci_dev;

    if (client_cb) {
        BteHciReadStoredLinkKeyReply reply;
        reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
        uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
        reply.max_keys = read_le16(data);
        /* We get the number of keys in this data buffer as well, but it's
         * better not to trust it and just return the number of responses that
         * we have actually received. */
        reply.num_keys = dev->stored_keys.num_responses;
        reply.stored_keys = dev->stored_keys.responses;
        BteHciReadStoredLinkKeyCb callback = client_cb;
        callback(hci, &reply, hci_userdata(hci));
    }

    _bte_hci_dev_install_event_handler(HCI_RETURN_LINK_KEYS, NULL, NULL);
    _bte_hci_dev_stored_keys_cleanup();
}

void bte_hci_read_stored_link_key(BteHci *hci, const BteBdAddr *address,
                                  BteHciReadStoredLinkKeyCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_STORED_LINK_KEY_OCF, HCI_HC_BB_OGF,
        HCI_R_STORED_LINK_KEY_PLEN, read_stored_link_key_cb, callback);
    if (UNLIKELY(!b)) return;

    _bte_hci_dev_stored_keys_cleanup();
    _bte_hci_dev_install_event_handler(HCI_RETURN_LINK_KEYS,
                                       return_link_keys_cb, hci);
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    if (address) {
        memcpy(data, address, sizeof(*address));
    }
    data[6] = address ? 0 : 1;
    _bte_hci_send_command(b);
}

static void write_stored_link_key_cb(BteHci *hci, BteBuffer *buffer,
                                     void *client_cb)
{
    if (!client_cb) return;
    BteHciWriteStoredLinkKeyReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    reply.num_keys = data[0];
    BteHciWriteStoredLinkKeyCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_write_stored_link_key(BteHci *hci, int num_keys,
                                   const BteHciStoredLinkKey *keys,
                                   BteHciWriteStoredLinkKeyCb callback)
{
    const uint8_t elem_size = sizeof(BteBdAddr) + sizeof(BteLinkKey);
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_STORED_LINK_KEY_OCF, HCI_HC_BB_OGF,
        HCI_CMD_HDR_LEN + num_keys * elem_size, write_stored_link_key_cb,
        callback);
    if (UNLIKELY(!b)) return;

    uint8_t *ptr_addr = b->data + HCI_CMD_HDR_LEN;
    uint8_t *ptr_key = ptr_addr + num_keys * sizeof(BteBdAddr);
    for (int i = 0; i < num_keys; i++) {
        const BteBdAddr *address = &keys[i].address;
        const BteLinkKey *key = &keys[i].key;
        memcpy(ptr_addr + i * sizeof(*address), address, sizeof(*address));
        memcpy(ptr_key + i * sizeof(*key), key, sizeof(*key));
    }
    _bte_hci_send_command(b);
}

static void delete_stored_link_key_cb(BteHci *hci, BteBuffer *buffer,
                                      void *client_cb)
{
    if (!client_cb) return;
    BteHciDeleteStoredLinkKeyReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    reply.num_keys = read_le16(data);
    BteHciDeleteStoredLinkKeyCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_delete_stored_link_key(BteHci *hci, const BteBdAddr *address,
                                    BteHciDeleteStoredLinkKeyCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_D_STORED_LINK_KEY_OCF, HCI_HC_BB_OGF,
        HCI_D_STORED_LINK_KEY_PLEN, delete_stored_link_key_cb, callback);
    if (UNLIKELY(!b)) return;

    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    if (address) {
        memcpy(data, address, sizeof(*address));
    }
    data[6] = address ? 0 : 1;
    _bte_hci_send_command(b);
}

void bte_hci_write_local_name(BteHci *hci, const char *name,
                              BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_LOCAL_NAME_OCF, HCI_HC_BB_OGF, HCI_W_LOCAL_NAME_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    strncpy((char *)b->data + HCI_CMD_HDR_LEN, name, HCI_MAX_NAME_LEN);
    _bte_hci_send_command(b);
}

static void read_local_name_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;

    BteHciReadLocalNameReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    strncpy(reply.name, (const char *)data, sizeof(reply.name) - 1);
    reply.name[sizeof(reply.name) - 1] = '\0';
    BteHciReadLocalNameCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_local_name(BteHci *hci, BteHciReadLocalNameCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_LOCAL_NAME_OCF, HCI_HC_BB_OGF, HCI_R_LOCAL_NAME_PLEN,
        read_local_name_cb, callback);
    _bte_hci_send_command(b);
}

static void read_page_timeout_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    const uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    BteHciReadPageTimeoutReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.page_timeout = read_le16(data);
    BteHciReadPageTimeoutCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_page_timeout(BteHci *hci, BteHciReadPageTimeoutCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_PAGE_TIMEOUT_OCF, HCI_HC_BB_OGF, HCI_R_PAGE_TIMEOUT_PLEN,
        read_page_timeout_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_page_timeout(BteHci *hci, uint16_t page_timeout,
                                BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_PAGE_TIMEOUT_OCF, HCI_HC_BB_OGF, HCI_W_PAGE_TIMEOUT_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(page_timeout, b->data + HCI_CMD_HDR_LEN);
    _bte_hci_send_command(b);
}

static void read_scan_enable_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    BteHciReadScanEnableReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.scan_enable = buffer->data[HCI_CMD_REPLY_POS_DATA];
    BteHciReadScanEnableCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_scan_enable(BteHci *hci, BteHciReadScanEnableCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_SCAN_EN_OCF, HCI_HC_BB_OGF, HCI_R_SCAN_EN_PLEN,
        read_scan_enable_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_scan_enable(BteHci *hci, uint8_t scan_enable,
                                BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_SCAN_EN_OCF, HCI_HC_BB_OGF, HCI_W_SCAN_EN_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = scan_enable;
    _bte_hci_send_command(b);
}

static void read_auth_enable_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    BteHciReadAuthEnableReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.auth_enable = buffer->data[HCI_CMD_REPLY_POS_DATA];
    BteHciReadAuthEnableCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_auth_enable(BteHci *hci, BteHciReadAuthEnableCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_AUTH_ENABLE_OCF, HCI_HC_BB_OGF, HCI_R_AUTH_ENABLE_PLEN,
        read_auth_enable_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_auth_enable(BteHci *hci, uint8_t auth_enable,
                               BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_AUTH_ENABLE_OCF, HCI_HC_BB_OGF, HCI_W_AUTH_ENABLE_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = auth_enable;
    _bte_hci_send_command(b);
}

static void read_class_of_device_cb(BteHci *hci, BteBuffer *buffer,
                                    void *client_cb)
{
    BteHciReadClassOfDeviceReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    memcpy(&reply.cod, buffer->data + HCI_CMD_REPLY_POS_DATA,
           sizeof(reply.cod));
    BteHciReadClassOfDeviceCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_class_of_device(BteHci *hci,
                                  BteHciReadClassOfDeviceCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_COD_OCF, HCI_HC_BB_OGF, HCI_R_COD_PLEN,
        read_class_of_device_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_class_of_device(BteHci *hci, const BteClassOfDevice *cod,
                                   BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_COD_OCF, HCI_HC_BB_OGF, HCI_W_COD_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    memcpy(b->data + HCI_CMD_HDR_LEN, cod, sizeof(*cod));
    _bte_hci_send_command(b);
}

static void read_auto_flush_timeout_cb(BteHci *hci, BteBuffer *buffer,
                                       void *client_cb)
{
    BteHciReadAutoFlushTimeoutReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    const uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    reply.conn_handle = read_le16(data);
    reply.flush_timeout = read_le16(data + 2);
    BteHciReadAutoFlushTimeoutCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_auto_flush_timeout(BteHci *hci,
                                     BteConnHandle conn_handle,
                                     BteHciReadAutoFlushTimeoutCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_FLUSHTO_OCF, HCI_HC_BB_OGF, HCI_R_FLUSHTO_PLEN,
        read_auto_flush_timeout_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(conn_handle, b->data + HCI_CMD_HDR_LEN);
    _bte_hci_send_command(b);
}

void bte_hci_write_auto_flush_timeout(BteHci *hci,
                                      BteConnHandle conn_handle,
                                      uint16_t timeout,
                                      BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_FLUSHTO_OCF, HCI_HC_BB_OGF, HCI_W_FLUSHTO_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(conn_handle, b->data + HCI_CMD_HDR_LEN);
    write_le16(timeout, b->data + HCI_CMD_HDR_LEN + 2);
    _bte_hci_send_command(b);
}

void bte_hci_set_ctrl_to_host_flow_control(BteHci *hci, uint8_t enable,
                                           BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_SET_HC_TO_H_FC_OCF, HCI_HC_BB_OGF, HCI_SET_HC_TO_H_FC_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = enable;
    _bte_hci_send_command(b);
}

void bte_hci_set_host_buffer_size(BteHci *hci,
                                  uint16_t acl_packet_len,
                                  uint16_t acl_packets,
                                  uint8_t sync_packet_len,
                                  uint16_t sync_packets,
                                  BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_HOST_BUF_SIZE_OCF, HCI_HC_BB_OGF, HCI_HOST_BUF_SIZE_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    write_le16(acl_packet_len, data);
    data[2] = sync_packet_len;
    write_le16(acl_packets, data + 3);
    write_le16(sync_packets, data + 5);
    _bte_hci_send_command(b);
}

static void read_current_iac_lap_cb(BteHci *hci, BteBuffer *buffer,
                                    void *client_cb)
{
    BteHciReadCurrentIacLapReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    const uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    reply.num_laps = data[0]; data++;
    /* We trust the controller that the reply len is long enough */
    BteLap *laps = malloc(sizeof(BteLap) * reply.num_laps);
    for (int i = 0; i < reply.num_laps; i++) {
        BteLap lap = data[0];
        lap |= ((uint32_t)data[1] << 8);
        lap |= ((uint32_t)data[2] << 16);
        data += 3;
        laps[i] = lap;
    }
    reply.laps = laps;
    BteHciReadCurrentIacLapCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
    free(laps);
}

void bte_hci_read_current_iac_lap(BteHci *hci,
                                  BteHciReadCurrentIacLapCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_CUR_IACLAP_OCF, HCI_HC_BB_OGF, HCI_R_CUR_IACLAP_PLEN,
        read_current_iac_lap_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_current_iac_lap(BteHci *hci,
                                   uint8_t num_laps, const BteLap *laps,
                                   BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_CUR_IACLAP_OCF, HCI_HC_BB_OGF,
        HCI_W_CUR_IACLAP_PLEN + 3 * num_laps,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    data[0] = num_laps; data++;
    for (int i = 0; i < num_laps; i++) {
        BteLap lap = laps[i];
        data[0] = lap & 0xff;
        data[1] = (lap >> 8) & 0xff;
        data[2] = (lap >> 16) & 0xff;
        data += 3;
    }
    _bte_hci_send_command(b);
}

void bte_hci_host_num_comp_packets(BteHci *hci,
                                   BteConnHandle conn_handle,
                                   uint16_t num_packets)
{
    BteBuffer *b = _bte_hci_dev_add_command_no_reply(
        HCI_HOST_NUM_COMPL_OCF, HCI_HC_BB_OGF, HCI_H_NUM_COMPL_PLEN);
    if (UNLIKELY(!b)) return;
    uint8_t *data = b->data + HCI_CMD_HDR_LEN;
    data[0] = 1;
    data++;
    write_le16(conn_handle, data);
    data += 2;
    write_le16(num_packets, data);
    _bte_hci_send_command(b);
}

static void read_link_sv_timeout_cb(BteHci *hci, BteBuffer *buffer,
                                       void *client_cb)
{
    BteHciReadLinkSvTimeoutReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    const uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;
    reply.conn_handle = read_le16(data);
    reply.sv_timeout = read_le16(data + 2);
    BteHciReadLinkSvTimeoutCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_link_sv_timeout(BteHci *hci,
                                     BteConnHandle conn_handle,
                                     BteHciReadLinkSvTimeoutCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_LINK_SV_TIMEOUT_OCF, HCI_HC_BB_OGF,
        HCI_R_LINK_SV_TIMEOUT_PLEN,
        read_link_sv_timeout_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(conn_handle, b->data + HCI_CMD_HDR_LEN);
    _bte_hci_send_command(b);
}

void bte_hci_write_link_sv_timeout(BteHci *hci,
                                   BteConnHandle conn_handle,
                                   uint16_t timeout,
                                   BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_LINK_SV_TIMEOUT_OCF, HCI_HC_BB_OGF,
        HCI_W_LINK_SV_TIMEOUT_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    write_le16(conn_handle, b->data + HCI_CMD_HDR_LEN);
    write_le16(timeout, b->data + HCI_CMD_HDR_LEN + 2);
    _bte_hci_send_command(b);
}

static void read_inquiry_scan_type_cb(BteHci *hci, BteBuffer *buffer,
                                      void *client_cb)
{
    BteHciReadInquiryScanTypeReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.inquiry_scan_type = buffer->data[HCI_CMD_REPLY_POS_DATA];
    BteHciReadInquiryScanTypeCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_inquiry_scan_type(BteHci *hci,
                                    BteHciReadInquiryScanTypeCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_INQUIRY_SCAN_TYPE_OCF, HCI_HC_BB_OGF, HCI_CMD_HDR_LEN,
        read_inquiry_scan_type_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_inquiry_scan_type(BteHci *hci, uint8_t inquiry_scan_type,
                                     BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_INQUIRY_SCAN_TYPE_OCF, HCI_HC_BB_OGF,
        HCI_W_INQUIRY_SCAN_TYPE_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = inquiry_scan_type;
    _bte_hci_send_command(b);
}

static void read_inquiry_mode_cb(BteHci *hci, BteBuffer *buffer,
                                 void *client_cb)
{
    BteHciReadInquiryModeReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.inquiry_mode = buffer->data[HCI_CMD_REPLY_POS_DATA];
    BteHciReadInquiryModeCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_inquiry_mode(BteHci *hci, BteHciReadInquiryModeCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_INQUIRY_MODE_OCF, HCI_HC_BB_OGF, HCI_CMD_HDR_LEN,
        read_inquiry_mode_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_inquiry_mode(BteHci *hci, uint8_t inquiry_mode,
                                BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_INQUIRY_MODE_OCF, HCI_HC_BB_OGF, HCI_W_INQUIRY_MODE_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = inquiry_mode;
    _bte_hci_send_command(b);
}

static void read_page_scan_type_cb(BteHci *hci, BteBuffer *buffer,
                                   void *client_cb)
{
    BteHciReadPageScanTypeReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.page_scan_type = buffer->data[HCI_CMD_REPLY_POS_DATA];
    BteHciReadPageScanTypeCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_page_scan_type(BteHci *hci,
                                 BteHciReadPageScanTypeCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_PAGE_SCAN_TYPE_OCF, HCI_HC_BB_OGF, HCI_CMD_HDR_LEN,
        read_page_scan_type_cb, callback);
    _bte_hci_send_command(b);
}

void bte_hci_write_page_scan_type(BteHci *hci, uint8_t page_scan_type,
                                  BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_PAGE_SCAN_TYPE_OCF, HCI_HC_BB_OGF,
        HCI_W_PAGE_SCAN_TYPE_PLEN,
        command_complete_cb, callback);
    if (UNLIKELY(!b)) return;
    b->data[HCI_CMD_HDR_LEN] = page_scan_type;
    _bte_hci_send_command(b);
}

static void read_local_version_cb(BteHci *hci, BteBuffer *buffer,
                                  void *client_cb)
{
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;

    BteHciReadLocalVersionReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.hci_version = data[0];
    reply.hci_revision = read_le16(data + 1);
    reply.lmp_version = data[3];
    reply.manufacturer = read_le16(data + 4);
    reply.lmp_subversion = read_le16(data + 6);
    BteHciReadLocalVersionCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_local_version(BteHci *hci,
                                BteHciReadLocalVersionCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(hci,
        HCI_R_LOC_VERS_INFO_OCF, HCI_INFO_PARAM_OGF, HCI_R_LOC_VERS_INFO_PLEN,
        read_local_version_cb, callback);
    _bte_hci_send_command(b);
}

static void read_local_features_cb(BteHci *hci, BteBuffer *buffer,
                                   void *client_cb)
{
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;

    BteHciReadLocalFeaturesReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.features = read_le64(data);
    BteHciReadLocalFeaturesCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_local_features(BteHci *hci,
                                 BteHciReadLocalFeaturesCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_LOC_FEAT_OCF, HCI_INFO_PARAM_OGF, HCI_R_LOC_FEAT_PLEN,
        read_local_features_cb, callback);
    _bte_hci_send_command(b);
}

static void read_buffer_size_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;

    BteHciReadBufferSizeReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    reply.acl_mtu = read_le16(data);
    reply.sco_mtu = data[2];
    reply.acl_max_packets = read_le16(data + 3);
    reply.sco_max_packets = read_le16(data + 5);
    BteHciReadBufferSizeCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_buffer_size(BteHci *hci, BteHciReadBufferSizeCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_BUF_SIZE_OCF, HCI_INFO_PARAM_OGF, HCI_R_BUF_SIZE_PLEN,
        read_buffer_size_cb, callback);
    _bte_hci_send_command(b);
}

static void read_bd_addr_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    uint8_t *data = buffer->data + HCI_CMD_REPLY_POS_DATA;

    BteHciReadBdAddrReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    memcpy(&reply.address, data, sizeof(reply.address));
    BteHciReadBdAddrCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
}

void bte_hci_read_bd_addr(BteHci *hci, BteHciReadBdAddrCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_R_BD_ADDR_OCF, HCI_INFO_PARAM_OGF, HCI_R_BD_ADDR_PLEN,
        read_bd_addr_cb, callback);
    _bte_hci_send_command(b);
}

static void vendor_command_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    BteHciVendorCommandCb callback = client_cb;
    callback(hci, buffer, hci_userdata(hci));
}

void bte_hci_vendor_command(BteHci *hci, uint16_t ocf,
                            const void *data, uint8_t len,
                            BteHciVendorCommandCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, ocf, HCI_VENDOR_OGF, HCI_CMD_HDR_LEN + len,
        vendor_command_cb, callback);
    if (UNLIKELY(!b)) return;
    memcpy(b->data + HCI_CMD_HDR_LEN, data, len);
    _bte_hci_send_command(b);
}

static bool client_handle_vendor_event(BteHci *hci, void *cb_data)
{
    BteBuffer *buffer = cb_data;
    return hci->vendor_event_cb &&
        hci->vendor_event_cb(hci, buffer, hci_userdata(hci));
}

static void vendor_event_cb(BteBuffer *buffer, void *cb_data)
{
    _bte_hci_dev_foreach_hci_client(client_handle_vendor_event, buffer);
}

void bte_hci_on_vendor_event(BteHci *hci, BteHciVendorEventCb callback)
{
    hci->vendor_event_cb = callback;
    _bte_hci_dev_install_event_handler(HCI_VENDOR_SPECIFIC_EVENT,
                                       vendor_event_cb, NULL);
}
