#include "hci.h"

#include "buffer.h"
#include "internals.h"
#include "logging.h"

#define HCI_CMD_REPLY_POS_CODE    0
#define HCI_CMD_REPLY_POS_LEN     1
#define HCI_CMD_REPLY_POS_HDR_LEN 2
#define HCI_CMD_REPLY_POS_PACKETS 2
#define HCI_CMD_REPLY_POS_OPCODE  3
#define HCI_CMD_REPLY_POS_STATUS  5
#define HCI_CMD_REPLY_POS_DATA    6

#define HCI_CMD_EVENT_POS_CODE 0
#define HCI_CMD_EVENT_POS_LEN  1
#define HCI_CMD_EVENT_POS_DATA 2

static void command_complete_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
    if (!client_cb) return;
    BteHciReply reply;
    reply.status = buffer->data[HCI_CMD_REPLY_POS_STATUS];
    BteHciDoneCb callback = client_cb;
    callback(hci, &reply, hci_userdata(hci));
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
    hci->initialized_cb = callback;
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

static void inquiry_status_cb(BteHci *hci, uint8_t status)
{
    if (status == 0) {
        _bte_hci_dev_install_event_handler(HCI_INQUIRY_RESULT,
                                           inquiry_result_cb, hci);
        _bte_hci_dev_install_event_handler(HCI_INQUIRY_COMPLETE,
                                           inquiry_event_cb, hci);
    } else {
        hci->inquiry_cb = NULL;
    }
}

void bte_hci_inquiry(BteHci *hci, uint32_t lap, uint8_t len, uint8_t max_resp,
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
                              uint32_t lap, uint8_t len, uint8_t max_resp,
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
