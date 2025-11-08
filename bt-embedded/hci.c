#include "hci.h"

#include "buffer.h"
#include "internals.h"
#include "logging.h"

#define HCI_CMD_REPLY_POS_CODE    0
#define HCI_CMD_REPLY_POS_LEN     1
#define HCI_CMD_REPLY_POS_PACKETS 2
#define HCI_CMD_REPLY_POS_OPCODE  3
#define HCI_CMD_REPLY_POS_STATUS  5
#define HCI_CMD_REPLY_POS_DATA    6

static void command_complete_cb(BteHci *hci, BteBuffer *buffer, void *client_cb)
{
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

void bte_hci_set_event_mask(BteHci *hci, BteHciEventMask mask,
                            BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_SET_EV_MASK_OCF, HCI_HC_BB_OGF, HCI_SET_EV_MASK_PLEN,
        command_complete_cb, callback);
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

void bte_hci_write_local_name(BteHci *hci, const char *name,
                              BteHciDoneCb callback)
{
    BteBuffer *b = _bte_hci_dev_add_pending_command(
        hci, HCI_W_LOCAL_NAME_OCF, HCI_HC_BB_OGF, HCI_W_LOCAL_NAME_PLEN,
        command_complete_cb, callback);
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
    memcpy(b->data + HCI_CMD_HDR_LEN, cod, sizeof(*cod));
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
