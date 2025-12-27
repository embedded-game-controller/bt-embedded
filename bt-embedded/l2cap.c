#include "l2cap.h"

#include "acl.h"
#include "buffer.h"
#include "client.h"
#include "hci.h"
#include "internals.h"
#include "l2cap_proto.h"
#include "logging.h"

#include <stdlib.h>
#include <sys/param.h>

typedef struct bte_acl_l2cap_t {
    BteAcl acl;

    BteL2cap *clients[BTE_ACL_MAX_CLIENTS];
} BteAclL2cap;

struct l2cap_configure_data_t {
    uint32_t rejected_mask;
    uint32_t unknown_mask;
    BteL2capConfigureParams params;
    /* When we are sending a response, this tells whether we have sent all
     * packets */
    bool has_pending_packets;
    uint8_t start_cmd;
};

#define L2CAP_CONFIG_PARSE_ERR_CORRUPTED (-1)
#define L2CAP_CONFIG_PARSE_ERR_TYPE      (-2)

/* Range 0000-003f is reserved */
static uint16_t s_next_local_channel_id = 0x0040;
static uint8_t s_last_signal_id = 0;

static inline BteAclL2cap *L(BteAcl *acl)
{
    return (BteAclL2cap *)acl;
}

static BteL2cap *l2cap_for_local_channel(BteAclL2cap *acl_l2cap,
                                         BteL2capChannelId channel_id)
{
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        BteL2cap *l2cap = acl_l2cap->clients[i];
        if (l2cap && l2cap->local_channel_id == channel_id)
            return l2cap;
    }
    return NULL;
}

static inline uint16_t next_local_channel_id()
{
    if (UNLIKELY(s_next_local_channel_id == 0)) {
        s_next_local_channel_id = 0x0040;
    }
    return s_next_local_channel_id++;
}

static inline uint16_t next_signal_id()
{
    if (UNLIKELY(s_last_signal_id == 0xff)) {
        s_last_signal_id = 0;
    }
    return ++s_last_signal_id;
}

static const BteHciConnectParams *default_connect_params(BteHci *hci)
{
    /* Taken from libogc */
    static BteHciConnectParams s_default_connect_params = {
        0, /* Packet type, computed dynamically below */
        BTE_HCI_CLOCK_OFFSET_INVALID,
        0x1, /* Assuming worst case: time between successive page scans
                starting <= 2.56s */
        true,
    };
    if (s_default_connect_params.packet_type == 0) {
        /* DRIVER This assumes that bte_hci_read_local_features() has already
         * been called! */
        BteHciFeatures features = bte_hci_get_supported_features(hci);
        s_default_connect_params.packet_type =
            bte_hci_packet_types_from_features(features);
    }
    return &s_default_connect_params;
}

static inline bool acl_l2cap_create_message(
    BteAclL2cap *acl_l2cap, BteBufferWriter *writer,
    uint16_t size, BteL2capChannelId channel_id)
{
    uint16_t total_size = L2CAP_HDR_LEN + size;
    bool ok = bte_acl_create_message(&acl_l2cap->acl, writer, total_size,
                                     BTE_ACL_BROADCAST_PTP);
    if (UNLIKELY(!ok)) return false;
    uint8_t *data =
        bte_buffer_writer_ptr_n(writer, L2CAP_HDR_LEN);
    /* Write the L2CAP header */
    write_le16(size, data);
    write_le16(channel_id, data + 2);
    return true;
}

static bool acl_l2cap_create_cmd(BteAclL2cap *acl_l2cap,
                                 BteBufferWriter *writer,
                                 uint8_t code, uint8_t id, uint16_t size)
{
    uint16_t cmd_size = L2CAP_SIGNAL_HDR_LEN + size;
    bool ok = acl_l2cap_create_message(acl_l2cap, writer, cmd_size,
                                       BTE_L2CAP_CHANNEL_ID_SIGNALLING);
    if (UNLIKELY(!ok)) return false;
    uint8_t *data =
        bte_buffer_writer_ptr_n(writer, L2CAP_SIGNAL_HDR_LEN);
    /* Write the signal header */
    data[0] = code;
    data[1] = id;
    write_le16(size, data + 2);
    return true;
}

static bool acl_l2cap_cmd_reply(
    BteAclL2cap *acl_l2cap, uint8_t code, uint8_t id, void *data, uint16_t len)
{
    BteBufferWriter writer;
    bool ok = acl_l2cap_create_cmd(acl_l2cap, &writer, code, id, len);
    if (UNLIKELY(!ok)) return false;
    bte_buffer_writer_write(&writer, data, len);

    bte_acl_send_message(&acl_l2cap->acl, bte_buffer_writer_end(&writer));
    return true;
}

static BteBuffer *acl_l2cap_signal(BteAclL2cap *acl_l2cap, uint8_t code,
                                   const void *payload, uint16_t size)
{
    BteBufferWriter writer;
    bool ok = acl_l2cap_create_cmd(acl_l2cap, &writer, code, next_signal_id(),
                                   size);
    if (UNLIKELY(!ok)) return NULL;
    /* Write the payload */
    uint8_t *data = bte_buffer_writer_ptr_n(&writer, size);
    memcpy(data, payload, size);

    return bte_buffer_writer_end(&writer);
}

static BteBuffer *l2cap_signal(BteL2cap *l2cap, uint8_t code,
                               const void *payload, uint16_t size)
{
    BteBuffer *buffer = acl_l2cap_signal(L(l2cap->acl), code, payload, size);
    if (LIKELY(buffer)) {
        l2cap->expected_response_code = code + 1;
        l2cap->expected_response_id = s_last_signal_id;
        l2cap->expected_response_count++;
    }
    return buffer;
}

static void l2cap_handle_error(BteL2cap *l2cap)
{
    /* The spec allows for various ways of handling these errors; one of them
     * being ignoring the packet with the error, which is what we do here. But
     * another option might be terminating the connection, and some clients
     * might prefer that.
     * This function exists so that in the future it might perform a different
     * action. */
}

static bool l2cap_connection_request(BteL2cap *l2cap)
{
    uint16_t data[2];
    data[0] = htole16(l2cap->psm);
    l2cap->local_channel_id = next_local_channel_id();
    data[1] = htole16(l2cap->local_channel_id);
    BteBuffer *buffer = l2cap_signal(l2cap, L2CAP_SIGNAL_CONN_REQ,
                                     data, sizeof(data));
    if (UNLIKELY(!buffer)) return false;

    return bte_acl_send_message(l2cap->acl, buffer) >= 0;
}

static void acl_l2cap_connected_cb(BteL2cap *l2cap, uint8_t status)
{
    /* We couldn't get an ACL connection or request a L2CAP one */
    if (UNLIKELY(status != 0 || !l2cap_connection_request(l2cap))) {
        BteL2capConnectCb client_cb =
            l2cap->last_async_cmd_data.connect.client_cb;
        BteL2capConnectionResponse reply = {
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE,
            BTE_L2CAP_CONN_RESP_STATUS_NO_INFO,
        };
        client_cb(NULL, &reply, l2cap->userdata);
        /* Destroy this object, no one holds a reference to it */
        bte_l2cap_unref(l2cap);
    }
}

static void acl_connected_cb(BteAcl *acl, uint8_t status)
{
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        BteL2cap *client = L(acl)->clients[i];
        if (client) {
            acl_l2cap_connected_cb(client, status);
        }
    }
}

static void l2cap_reject_command_invalid_cid(
    BteAclL2cap *acl_l2cap, uint8_t id,
    BteL2capChannelId channel_id, BteL2capChannelId remote_channel_id)
{
    uint16_t data[2];
    data[0] = htole16(channel_id);
    data[1] = htole16(remote_channel_id);
    acl_l2cap_cmd_reply(acl_l2cap, L2CAP_SIGNAL_CMD_REJ, id, data, sizeof(data));
}

static bool l2cap_connect_cb(BteL2cap *l2cap, BteBufferReader *reader,
                             uint16_t resp_len)
{
    uint16_t data[4];

    if (UNLIKELY(resp_len < sizeof(data))) {
        BTE_WARN("%s response size %d, needed size %zu\n",
                 __func__, resp_len, sizeof(data));
        return false;
    }
    uint16_t len;
    if ((len = bte_buffer_reader_read(reader, data, resp_len)) != resp_len) {
        BTE_WARN("%s response size %d, actual size %d\n",
                 __func__, resp_len, len);
        return false;
    }

    BteL2capConnectionResponse reply;
    reply.result = le16toh(data[2]);
    if (reply.result > BTE_L2CAP_CONN_RESP_RES_PENDING) {
        /* An error occurred */
        reply.remote_channel_id = reply.local_channel_id = 0;
        reply.status = BTE_L2CAP_CONN_RESP_STATUS_NO_INFO;
    } else {
        reply.remote_channel_id = le16toh(data[0]);
        reply.local_channel_id = le16toh(data[1]);
        reply.status = le16toh(data[3]);

        l2cap->remote_channel_id = reply.remote_channel_id;
        if (UNLIKELY(reply.local_channel_id != l2cap->local_channel_id)) {
            BTE_WARN("%s local CID mismatch: %04x != %04x!\n", __func__,
                     reply.local_channel_id, l2cap->local_channel_id);
            return false;
        }
    }

    BteL2capConnectCb client_cb = l2cap->last_async_cmd_data.connect.client_cb;
    client_cb(l2cap, &reply, l2cap->userdata);
    if (reply.result != BTE_L2CAP_CONN_RESP_RES_PENDING) {
        /* No more callbacks will be invoked. The client should have taken its
         * reference to the l2cap opject; if not, it will be destroyed here */
        bte_l2cap_unref(l2cap);
    }

    return true;
}

static void l2cap_config_params_clear(BteL2capConfigureParams *params)
{
    free((BteL2capConfigQos *)params->qos);
    free((BteL2capConfigRetxFlow *)params->retx_flow);
    free((BteL2capConfigExtFlow *)params->ext_flow);
    params->qos = NULL;
    params->retx_flow = NULL;
    params->ext_flow = NULL;
    params->field_mask = 0;
}

/* Returns:
 *  0 if all OK
 * -1 if the message is corrupted
 * -2 on unkown type (the unknown_size argument is filled with the total
 *    size of the unknown parameters)
 */
static int l2cap_config_parse(
    BteBufferReader *reader, BteL2capConfigureParams *params,
    uint16_t cmd_size, uint16_t *unknown_size)
{
    uint16_t read = 0;
    int error_return = L2CAP_CONFIG_PARSE_ERR_CORRUPTED;

    while (read < cmd_size) {
        uint8_t header[2];
        uint16_t len = bte_buffer_reader_read(reader, header, sizeof(header));
        if (UNLIKELY(len != sizeof(header))) goto error;
        read += 2;

        uint8_t type = header[0];
        uint8_t size = header[1];
        if (UNLIKELY(read + size > cmd_size)) goto error;
        uint8_t expected_size = 0;
        uint16_t *val_u16 = NULL;
        uint8_t *val_u8 = NULL;
        switch (type & 0x7f) {
        case L2CAP_CONFIG_MTU:
            params->field_mask |= BTE_L2CAP_CONFIG_MTU;
            val_u16 = &params->mtu;
            break;
        case L2CAP_CONFIG_FLUSH_TIMEOUT:
            params->field_mask |= BTE_L2CAP_CONFIG_FLUSH_TIMEOUT;
            val_u16 = &params->flush_timeout;
            break;
        case L2CAP_CONFIG_FRAME_CHECK_SEQ:
            params->field_mask |= BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ;
            val_u8 = &params->frame_check_sequence;
            break;
        case L2CAP_CONFIG_MAX_WINDOW_SIZE:
            params->field_mask |= BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE;
            val_u16 = &params->max_window_size;
            break;
        case L2CAP_CONFIG_QOS:
            params->field_mask |= BTE_L2CAP_CONFIG_QOS;
            expected_size = L2CAP_CONFIG_QOS_LEN;
            break;
        case L2CAP_CONFIG_RETX_FLOW:
            params->field_mask |= BTE_L2CAP_CONFIG_RETX_FLOW;
            expected_size = L2CAP_CONFIG_RETX_FLOW_LEN;
            break;
        case L2CAP_CONFIG_EXT_FLOW:
            params->field_mask |= BTE_L2CAP_CONFIG_EXT_FLOW;
            expected_size = L2CAP_CONFIG_EXT_FLOW_LEN;
            break;
        default:
            BTE_WARN("Unknown config type %02x\n", type);
            if (!(type & L2CAP_CONFIG_HINT)) {
                error_return = L2CAP_CONFIG_PARSE_ERR_TYPE;
                if (unknown_size) *unknown_size += sizeof(header) + size;
            }
            bte_buffer_reader_advance(reader, size);
            read += size;
            continue;
        }

        void *param_data = NULL;
        if (val_u16) {
            len = bte_buffer_reader_read(reader, val_u16, 2);
            expected_size = 2;
            *val_u16 = le16toh(*val_u16);
        } else if (val_u8) {
            expected_size = 1;
            len = bte_buffer_reader_read(reader, val_u8, expected_size);
        } else {
            param_data = malloc(expected_size);
            len = bte_buffer_reader_read(reader, param_data, expected_size);
        }

        if (UNLIKELY(len != expected_size || size != expected_size))
            goto error;

        if (param_data) {
            switch (type) {
            case L2CAP_CONFIG_QOS:
                {
                    BteL2capConfigQos *p = param_data;
                    p->token_rate = le32toh(p->token_rate);
                    p->token_bucket_size = le32toh(p->token_bucket_size);
                    p->peak_bandwith = le32toh(p->peak_bandwith);
                    p->access_latency = le32toh(p->access_latency);
                    p->delay_variation = le32toh(p->delay_variation);
                    free((void*)params->qos);
                    params->qos = p;
                }
                break;
            case L2CAP_CONFIG_RETX_FLOW:
                {
                    BteL2capConfigRetxFlow *p = param_data;
                    p->retx_timeout = le16toh(p->retx_timeout);
                    p->monitor_timeout = le16toh(p->monitor_timeout);
                    p->max_pdu_size = le16toh(p->max_pdu_size);
                    free((void*)params->retx_flow);
                    params->retx_flow = p;
                }
                break;
            case L2CAP_CONFIG_EXT_FLOW:
                {
                    BteL2capConfigExtFlow *p = param_data;
                    p->max_sdu_size = le16toh(p->max_sdu_size);
                    p->sdu_inter_time = le32toh(p->sdu_inter_time);
                    p->access_latency = le32toh(p->access_latency);
                    p->flush_timeout = le32toh(p->flush_timeout);
                    free((void*)params->ext_flow);
                    params->ext_flow = p;
                }
                break;
            }
        }

        read += size;
    }
    if (unknown_size && *unknown_size) goto error;
    return 0;

error:
    l2cap_config_params_clear(params);
    return error_return;
}

/* Returns 0 if all options could be stored in buffer, otherwise the command ID
 * of the first command which didn't fit */
static uint8_t l2cap_config_write(const BteL2capConfigureParams *params,
                                  uint8_t start_command,
                                  uint8_t *buffer, uint16_t buffer_size,
                                  uint16_t *used_size)
{
    uint16_t size = 0;
    uint8_t overflow_cmd = 0;

    if (start_command <= L2CAP_CONFIG_MTU &&
        params->field_mask & BTE_L2CAP_CONFIG_MTU) {
        if (size + 2 + L2CAP_CONFIG_MTU_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_MTU;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_MTU;
                buffer[1] = L2CAP_CONFIG_MTU_LEN;
                buffer += 2;
                write_le16(params->mtu, buffer);
                buffer += 2;
            }
            size += 2 + L2CAP_CONFIG_MTU_LEN;
        }
    }

    if (start_command <= L2CAP_CONFIG_FLUSH_TIMEOUT &&
        params->field_mask & BTE_L2CAP_CONFIG_FLUSH_TIMEOUT) {
        if (size + 2 + L2CAP_CONFIG_FLUSH_TIMEOUT_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_FLUSH_TIMEOUT;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_FLUSH_TIMEOUT;
                buffer[1] = L2CAP_CONFIG_FLUSH_TIMEOUT_LEN;
                buffer += 2;
                write_le16(params->flush_timeout, buffer);
                buffer += 2;
            }
            size += 2 + L2CAP_CONFIG_FLUSH_TIMEOUT_LEN;
        }
    }

    if (start_command <= L2CAP_CONFIG_QOS && params->qos) {
        if (size + 2 + L2CAP_CONFIG_QOS_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_QOS;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_QOS;
                buffer[1] = L2CAP_CONFIG_QOS_LEN;
                buffer[2] = params->qos->flags;
                buffer[3] = params->qos->service_type;
                buffer += 4;
                write_le32(params->qos->token_rate, buffer);
                buffer += 4;
                write_le32(params->qos->token_bucket_size, buffer);
                buffer += 4;
                write_le32(params->qos->peak_bandwith, buffer);
                buffer += 4;
                write_le32(params->qos->access_latency, buffer);
                buffer += 4;
                write_le32(params->qos->delay_variation, buffer);
                buffer += 4;
            }
            size += 2 + L2CAP_CONFIG_QOS_LEN;
        }
    }

    if (start_command <= L2CAP_CONFIG_RETX_FLOW && params->retx_flow) {
        if (size + 2 + L2CAP_CONFIG_RETX_FLOW_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_RETX_FLOW;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_RETX_FLOW;
                buffer[1] = L2CAP_CONFIG_RETX_FLOW_LEN;
                buffer[2] = params->retx_flow->mode;
                buffer[3] = params->retx_flow->tx_window_size;
                buffer[4] = params->retx_flow->max_transmit;
                buffer += 5;
                write_le16(params->retx_flow->retx_timeout, buffer);
                buffer += 2;
                write_le16(params->retx_flow->monitor_timeout, buffer);
                buffer += 2;
                write_le16(params->retx_flow->max_pdu_size, buffer);
                buffer += 2;
            }
            size += 2 + L2CAP_CONFIG_RETX_FLOW_LEN;
        }
    }

    if (start_command <= L2CAP_CONFIG_FRAME_CHECK_SEQ &&
        params->field_mask & BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ) {
        if (size + 2 + L2CAP_CONFIG_FRAME_CHECK_SEQ_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_FRAME_CHECK_SEQ;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_FRAME_CHECK_SEQ;
                buffer[1] = L2CAP_CONFIG_FRAME_CHECK_SEQ_LEN;
                buffer[2] = params->frame_check_sequence;
                buffer += 3;
            }
            size += 2 + L2CAP_CONFIG_FRAME_CHECK_SEQ_LEN;
        }
    }

    if (start_command <= L2CAP_CONFIG_EXT_FLOW && params->ext_flow) {
        if (size + 2 + L2CAP_CONFIG_EXT_FLOW_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_EXT_FLOW;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_EXT_FLOW;
                buffer[1] = L2CAP_CONFIG_EXT_FLOW_LEN;
                buffer[2] = params->ext_flow->identifier;
                buffer[3] = params->ext_flow->service_type;
                buffer += 4;
                write_le16(params->ext_flow->max_sdu_size, buffer);
                buffer += 2;
                write_le32(params->ext_flow->sdu_inter_time, buffer);
                buffer += 4;
                write_le32(params->ext_flow->access_latency, buffer);
                buffer += 4;
                write_le32(params->ext_flow->flush_timeout, buffer);
                buffer += 4;
            }
            size += 2 + L2CAP_CONFIG_EXT_FLOW_LEN;
        }
    }

    if (start_command <= L2CAP_CONFIG_MAX_WINDOW_SIZE &&
        params->field_mask & BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE) {
        if (size + 2 + L2CAP_CONFIG_MAX_WINDOW_SIZE_LEN > buffer_size) {
            overflow_cmd = L2CAP_CONFIG_MAX_WINDOW_SIZE;
            goto end;
        } else {
            if (buffer) {
                buffer[0] = L2CAP_CONFIG_MAX_WINDOW_SIZE;
                buffer[1] = L2CAP_CONFIG_MAX_WINDOW_SIZE_LEN;
                buffer += 2;
                write_le16(params->max_window_size, buffer);
                buffer += 2;
            }
            size += 2 + L2CAP_CONFIG_MAX_WINDOW_SIZE_LEN;
        }
    }

end:
    if (used_size) *used_size = size;
    return overflow_cmd;
}

static bool l2cap_config_send(BteL2cap *l2cap, L2capConfigureData *conf,
                              uint8_t msg_id)
{
    uint8_t code;
    uint16_t header_size, result;

    uint8_t start_cmd = 0;
    if (msg_id) {
        code = L2CAP_SIGNAL_CONFIG_RSP;
        header_size = L2CAP_CONFIG_RSP_HDR_LEN;
        result = conf->rejected_mask != 0 ?
            L2CAP_CONFIG_RES_ERR_REJ : L2CAP_CONFIG_RES_OK;
        if (conf->has_pending_packets) {
            start_cmd = conf->start_cmd;
            conf->has_pending_packets = false;
        }
    } else {
        msg_id = next_signal_id();
        code = L2CAP_SIGNAL_CONFIG_REQ;
        header_size = L2CAP_CONFIG_REQ_HDR_LEN;
    }

    bool ok = true;
    do {
        uint16_t size = 0;
        uint16_t new_start_cmd =
            l2cap_config_write(&conf->params, start_cmd, NULL,
                               l2cap->remote_mtu, &size);
        uint16_t flags = new_start_cmd > 0 ?
            L2CAP_CONFIG_FLAG_CONTINUATION : 0;

        BteBufferWriter writer;
        uint16_t cmd_size = header_size + size;
        acl_l2cap_create_cmd(L(l2cap->acl), &writer, code,
                             msg_id, cmd_size);
        uint8_t *ptr = bte_buffer_writer_ptr_n(&writer, cmd_size);
        /* TODO: handle !ptr */
        write_le16(l2cap->remote_channel_id, ptr);
        ptr += 2;
        write_le16(flags, ptr);
        ptr += 2;
        if (code == L2CAP_SIGNAL_CONFIG_RSP) {
            write_le16(result, ptr);
            ptr += 2;
        }
        if (size > 0) {
            l2cap_config_write(&conf->params, start_cmd, ptr,
                               l2cap->remote_mtu, NULL);
        }
        BteBuffer *buffer = bte_buffer_writer_end(&writer);
        ok = bte_acl_send_message(l2cap->acl, buffer);
        if (!ok) break;

        if (code == L2CAP_SIGNAL_CONFIG_REQ) {
            l2cap->expected_response_code = L2CAP_SIGNAL_CONFIG_RSP;
            if (l2cap->expected_response_count == 0)
                l2cap->expected_response_id = s_last_signal_id;
            l2cap->expected_response_count++;
            if (new_start_cmd > 0) msg_id = next_signal_id();
        } else if (new_start_cmd > 0) {
            /* We should wait for an empty request:
             * When used in the Configuration Response, the continuation flag
             * shall be set to one if the flag is set to one in the Request. If
             * the continuation flag is set to one in the Response when the
             * matching Request has the flag set to zero, it indicates the
             * responder has additional options to send to the requestor. In
             * this situation, the requestor shall send null-option
             * Configuration Requests (with continuation flag set to zero) to
             * the responder until the responder replies with a Configuration
             * Response where the continuation flag is set to zero.
             */
            conf->has_pending_packets = true;
            conf->start_cmd = new_start_cmd;
            break;
        }
        start_cmd = new_start_cmd;
    } while (start_cmd > 0);

    return ok;
}

static bool l2cap_config_send_resp_packet(BteL2cap *l2cap, uint8_t msg_id)
{
    L2capConfigureData *conf = l2cap->configure_req;
    bool ok = l2cap_config_send(l2cap, conf, msg_id);
    if (!conf->has_pending_packets) {
        l2cap_config_params_clear(&conf->params);
        free(conf);
        l2cap->configure_req = NULL;
    }
    return ok;
}

static bool l2cap_handle_config_resp(BteL2cap *l2cap, BteBufferReader *reader,
                                     uint16_t resp_len)
{
    uint16_t header[3];
    if (UNLIKELY(resp_len < sizeof(header))) {
        BTE_WARN("%s response size %d, needed size %zu\n",
                 __func__, resp_len, sizeof(header));
        return false;
    }

    uint16_t len;
    if (UNLIKELY((len = bte_buffer_reader_read(reader, header, sizeof(header)))
                 != sizeof(header))) {
        BTE_WARN("%s response size %d, actual size %d\n",
                 __func__, resp_len, len);
        return false;
    }
    uint16_t flags = le16toh(header[1]);
    L2capConfigureData conf;
    if (!l2cap->configure_resp) {
        if (flags & L2CAP_CONFIG_FLAG_CONTINUATION) {
            l2cap->configure_resp = malloc(sizeof(L2capConfigureData));
        } else {
            l2cap->configure_resp = &conf;
        }
        memset(l2cap->configure_resp, 0, sizeof(L2capConfigureData));
    }
    BteL2capConfigureParams *params = &l2cap->configure_resp->params;

    uint16_t result = le16toh(header[2]);
    if (result == L2CAP_CONFIG_RES_ERR_PARAMS) {
        /* In this case the field_mask will contain the elements that were
         * rejected. Save the current mask, which might have been set by a
         * previous packet (if this is a continued request) */
        uint32_t saved_mask = params->field_mask;
        params->field_mask = l2cap->configure_resp->rejected_mask;
        l2cap_config_parse(reader, params, resp_len - sizeof(header), NULL);
        l2cap->configure_resp->rejected_mask = params->field_mask;
        params->field_mask |= saved_mask;
    } else if (result == L2CAP_CONFIG_RES_ERR_UNKNOWN) {
        BteL2capConfigureParams tmp_params = { 0, };
        l2cap_config_parse(reader, &tmp_params, resp_len - sizeof(header),
                           NULL);
        l2cap->configure_resp->unknown_mask = tmp_params.field_mask;
    } else if (result == L2CAP_CONFIG_RES_OK) {
        l2cap_config_parse(reader, params, resp_len - sizeof(header), NULL);
    }

    if (!flags & L2CAP_CONFIG_FLAG_CONTINUATION) {
        /* No more messages following; we can deliver the reply to the
         * client */
        BteL2capConfigureReply reply;
        reply.unknown_mask = l2cap->configure_resp->unknown_mask;
        reply.rejected_mask = l2cap->configure_resp->rejected_mask;
        reply.params = l2cap->configure_resp->params;
        BteL2capConfigureCb client_cb =
            l2cap->last_async_cmd_data.configure.client_cb;
        client_cb(l2cap, &reply, l2cap->last_async_cmd_data.configure.userdata);
        if (l2cap->configure_resp) {
            l2cap_config_params_clear(&l2cap->configure_resp->params);
            if (l2cap->configure_resp != &conf) {
                free(l2cap->configure_resp);
            }
            l2cap->configure_resp = NULL;
        }
    } else if (l2cap->expected_response_count == 1) {
        /* If this was supposed to be the last response packet, but it has the
         * continuation flag set, let's send a null-option packet with a new ID
         */
        L2capConfigureData empty = { 0, };
        l2cap_config_send(l2cap, &empty, 0);
    }
    return true;
}

static bool l2cap_configure_type_is_known(uint8_t type)
{
    /* Hints are not critical */
    if (type & L2CAP_CONFIG_HINT) return true;

    switch (type & 0x7f) {
    case L2CAP_CONFIG_MTU:
    case L2CAP_CONFIG_FLUSH_TIMEOUT:
    case L2CAP_CONFIG_FRAME_CHECK_SEQ:
    case L2CAP_CONFIG_MAX_WINDOW_SIZE:
    case L2CAP_CONFIG_QOS:
    case L2CAP_CONFIG_RETX_FLOW:
    case L2CAP_CONFIG_EXT_FLOW:
        return true;
    default:
        return false;
    }
}

static void l2cap_configure_reply_type_error(
    BteL2cap *l2cap, uint8_t id, BteBufferReader *reader, uint16_t req_len,
    uint16_t reply_size)
{
    /* Re-parse the request, but this time just copy the unrecognized types */
    uint16_t header[2];
    /* No error checking, this was parsed before */
    bte_buffer_reader_read(reader, header, sizeof(header));
    uint16_t flags = le16toh(header[1]);

    uint16_t cmd_size = req_len - sizeof(header);

    BteBufferWriter writer;
    acl_l2cap_create_cmd(L(l2cap->acl), &writer, L2CAP_SIGNAL_CONFIG_RSP,
                         id, L2CAP_CONFIG_RSP_HDR_LEN + reply_size);
    uint8_t *data =
        bte_buffer_writer_ptr_n(&writer, L2CAP_CONFIG_RSP_HDR_LEN);
    write_le16(l2cap->remote_channel_id, data);
    write_le16(flags, data + 2);
    uint16_t result = L2CAP_CONFIG_RES_ERR_UNKNOWN;
    write_le16(result, data + 4);

    /* Now copy all the unrecognized parameters into the reply */
    uint16_t read = 0;
    while (read < cmd_size) {
        uint8_t header[2];
        bte_buffer_reader_read(reader, header, sizeof(header));
        read += 2;

        uint8_t type = header[0];
        uint8_t param_size = header[1];

        if (!l2cap_configure_type_is_known(type)) {
            bte_buffer_writer_write(&writer, header, sizeof(header));
            uint8_t param_read = 0;
            while (param_read < param_size) {
                uint8_t buffer[32];
                uint16_t len = MIN(param_size, sizeof(buffer));
                bte_buffer_reader_read(reader, buffer, len);
                bte_buffer_writer_write(&writer, buffer, len);
                param_read += len;
            }
        } else {
            bte_buffer_reader_advance(reader, param_size);
        }

        read += param_size;
    }

    BteBuffer *buffer = bte_buffer_writer_end(&writer);
    bte_acl_send_message(l2cap->acl, buffer);
    return;
}

static void l2cap_config_validate(BteL2cap *l2cap, L2capConfigureData *conf)
{
    /* TODO */
}

static void l2cap_config_apply(BteL2cap *l2cap,
                               const BteL2capConfigureParams *params)
{
    if (params->field_mask & BTE_L2CAP_CONFIG_MTU) {
        l2cap->remote_mtu = params->mtu;
    }
    /* TODO: handle the other parameters */
}

static bool acl_l2cap_handle_configure_req(
    BteAclL2cap *acl_l2cap, uint8_t id,
    BteBufferReader *reader, uint16_t req_len)
{
    if (UNLIKELY(req_len < L2CAP_CONFIG_REQ_HDR_LEN)) return false;

    /* Create a copy of the reader: in case of unrecognized parameters we'll be
     * parsing the buffer twice */
    BteBufferReader reader_copy = *reader;

    uint16_t header[2];
    uint16_t len = bte_buffer_reader_read(reader, header, sizeof(header));
    if (UNLIKELY(len != sizeof(header))) return false;

    uint16_t channel_id = le16toh(header[0]);
    uint16_t flags = le16toh(header[1]);

    BteL2cap *l2cap = NULL;
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        BteL2cap *client = acl_l2cap->clients[i];
        if (client && client->local_channel_id == channel_id) {
            l2cap = client;
            break;
        }
    }

    if (UNLIKELY(!l2cap)) {
        l2cap_reject_command_invalid_cid(acl_l2cap, id, channel_id,
                                         BTE_L2CAP_CHANNEL_ID_NULL);
        return true;
    }

    L2capConfigureData conf_store;
    L2capConfigureData *conf = l2cap->configure_req;
    if (!conf) {
        if (flags & L2CAP_CONFIG_FLAG_CONTINUATION ||
            /* The configuration response can be up to 2 bytes longer than the
             * request, since its header longer. If there is a risk of
             * fragmentation, make sure we allocate the conf data on the heap
             */
            req_len + 2 > l2cap->remote_mtu) {
            conf = malloc(sizeof(L2capConfigureData));
        } else {
            conf = &conf_store;
        }
        memset(conf, 0, sizeof(L2capConfigureData));
        l2cap->configure_req = conf;
    }
    BteL2capConfigureParams *params = &conf->params;

    uint16_t unknown_param_size = 0;
    if (conf->has_pending_packets) {
        /* We only expect null-option requests, to have an ID to which we
         * should send out continuation reply */
        l2cap_config_send_resp_packet(l2cap, id);
        return true;
    }

    int rc = l2cap_config_parse(reader, params, req_len - sizeof(header),
                                &unknown_param_size);
    if (UNLIKELY(rc == L2CAP_CONFIG_PARSE_ERR_TYPE)) {
        l2cap_configure_reply_type_error(l2cap, id, &reader_copy,
                                         req_len, unknown_param_size);
    } else if (UNLIKELY(rc == L2CAP_CONFIG_PARSE_ERR_CORRUPTED)) {
        l2cap_handle_error(l2cap);
    } else if (flags & L2CAP_CONFIG_FLAG_CONTINUATION) {
        /* Just ack the message */
        uint16_t ack[3];
        ack[0] = htole16(l2cap->remote_channel_id);
        ack[1] = htole16(L2CAP_CONFIG_FLAG_CONTINUATION);
        ack[2] = htole16(L2CAP_CONFIG_RES_OK);
        acl_l2cap_cmd_reply(acl_l2cap, L2CAP_SIGNAL_CONFIG_RSP, id,
                            ack, sizeof(ack));
    } else {
        /* All went OK, and no more messages following; we can deliver the
         * reply to the client */
        if (l2cap->configure_cb) {
            l2cap->configure_cb(l2cap, params, l2cap->configure_userdata);
        }
        /* This sets the rejected mask as needed */
        l2cap_config_validate(l2cap, conf);
        if (conf->rejected_mask == 0) {
            l2cap_config_apply(l2cap, &conf->params);
        }

        l2cap_config_send(l2cap, conf, id);
    }

    if (conf->has_pending_packets) {
        if (UNLIKELY(conf == &conf_store)) {
            /* We need to store the configuration on the heap, since we'll be
             * using later too */
            l2cap->configure_req = conf = malloc(sizeof(conf_store));
            memcpy(conf, &conf_store, sizeof(conf_store));
        }
    } else if (!(flags & L2CAP_CONFIG_FLAG_CONTINUATION)) {
        /* Cleanup the temporary structures */
        l2cap_config_params_clear(&conf->params);
        if (l2cap->configure_req != &conf_store) {
            free(l2cap->configure_req);
        }
        l2cap->configure_req = NULL;
    }

    return true;
}

static bool acl_l2cap_handle_request(
    BteAclL2cap *acl_l2cap, uint8_t code, uint8_t id,
    BteBufferReader *reader, uint16_t req_len)
{
    bool ok = false;
    switch (code) {
    case L2CAP_SIGNAL_CONFIG_REQ:
        ok = acl_l2cap_handle_configure_req(acl_l2cap, id, reader, req_len);
        break;
    /* TODO */
    }
    return ok;
}

static bool l2cap_handle_response(BteL2cap *l2cap, uint8_t code,
                                  BteBufferReader *reader, uint16_t resp_len)
{
    bool ok = false;
    switch (code) {
    case L2CAP_SIGNAL_CONN_RSP:
        ok = l2cap_connect_cb(l2cap, reader, resp_len);
        break;
    case L2CAP_SIGNAL_CONFIG_RSP:
        ok = l2cap_handle_config_resp(l2cap, reader, resp_len);
        break;
    /* TODO */
    }
    return ok;
}

static bool acl_l2cap_handle_response(
    BteAclL2cap *acl_l2cap, uint8_t code, uint8_t id,
    BteBufferReader *reader, uint16_t cmd_len)
{
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        BteL2cap *l2cap = acl_l2cap->clients[i];
        if (!l2cap || l2cap->expected_response_count == 0) continue;

        if (id != l2cap->expected_response_id) continue;
        if (code == l2cap->expected_response_code ||
            code == L2CAP_SIGNAL_CMD_REJ) {
            /* Temporary reference, since l2cap_handle_response() could destroy
             * the l2cap object when handling the connect response, if the
             * client doesn't care about it */
            bte_l2cap_ref(l2cap);
            bool ok = l2cap_handle_response(l2cap, code, reader, cmd_len);
            l2cap->expected_response_count--;
            if (l2cap->expected_response_count)
                l2cap->expected_response_id++;
            bte_l2cap_unref(l2cap);
            return ok;
        }
    }
    /* Ignore this message */
    return bte_buffer_reader_advance(reader, cmd_len) == cmd_len;
}

static void acl_data_received_cb(BteAcl *acl, BteBufferReader *reader)
{
    const uint16_t *header = bte_buffer_reader_read_n(reader, L2CAP_HDR_LEN);
    uint16_t total_len = le16toh(header[0]);
    BteL2capChannelId channel_id = le16toh(header[1]);

    if (channel_id == BTE_L2CAP_CHANNEL_ID_SIGNALLING) {
        /* There might be multiple commands in the buffer */
        uint16_t parsed_len, cmd_len;
        for (parsed_len = 0; parsed_len < total_len; parsed_len += cmd_len) {
            uint8_t sig_hdr[L2CAP_SIGNAL_HDR_LEN];
            uint16_t read =
                bte_buffer_reader_read(reader, sig_hdr, sizeof(sig_hdr));
            /* Did the peer send us a corrupted message? */
            if (UNLIKELY(read != sizeof(sig_hdr))) break;

            parsed_len += sizeof(sig_hdr);
            uint8_t code = sig_hdr[0];
            uint8_t id = sig_hdr[1];
            cmd_len = read_le16(sig_hdr + 2);
            if (code % 2 == 0) {
                /* It's a request */
                acl_l2cap_handle_request(L(acl), code, id, reader, cmd_len);
            } else {
                acl_l2cap_handle_response(L(acl), code, id, reader, cmd_len);
            }
        }
    } else {
        BteL2cap *l2cap = l2cap_for_local_channel(L(acl), channel_id);
        if (UNLIKELY(!l2cap)) {
            /* TODO double check: no errors should be sent here? */
            return;
        }
        /* TODO: this message contains data for the client, invoke the callback */
    }
}

static bool acl_l2cap_add_client(BteAclL2cap *acl_l2cap, BteL2cap *client)
{
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        if (!acl_l2cap->clients[i]) {
            acl_l2cap->clients[i] = client;
            return true;
        }
    }
    return false;
}

static void acl_l2cap_remove_client(BteAclL2cap *acl_l2cap, BteL2cap *client)
{
    int active_clients = 0;
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        if (acl_l2cap->clients[i]) {
            if (acl_l2cap->clients[i] == client) {
                acl_l2cap->clients[i] = NULL;
            } else {
                active_clients++;
            }
        }
    }

    if (active_clients == 0) {
        bte_acl_disconnect(&acl_l2cap->acl);
    }
}

static void bte_l2cap_free(BteL2cap *l2cap)
{
    if (l2cap->acl) {
        acl_l2cap_remove_client(L(l2cap->acl), l2cap);
        bte_acl_unref(l2cap->acl);
    }
    free(l2cap);
}

BteL2cap *bte_l2cap_new()
{
    BteL2cap *l2cap = malloc(sizeof(BteL2cap));
    memset(l2cap, 0, sizeof(BteL2cap));
    l2cap->ref_count = 1;
    l2cap->mtu = L2CAP_MTU_DEFAULT;
    l2cap->remote_mtu = L2CAP_MTU_MIN;
    return l2cap;
}

void bte_l2cap_connect(BteHci *hci, const BteBdAddr *address, BteL2capPsm psm,
                       const BteHciConnectParams *params,
                       BteL2capConnectCb callback, void *userdata)
{
    BteL2cap *l2cap = bte_l2cap_new();

    /* TODO: can we have more than one connection to the same address/psm? If
     * not, we should check for duplicates here... */
    BteAcl *acl = bte_acl_get_for_address(hci, address);
    if (acl) {
        l2cap->acl = bte_acl_ref(acl);
    } else {
        acl = bte_acl_new(hci, address, sizeof(BteAclL2cap));
        l2cap->acl = acl;
        acl->connected_cb = acl_connected_cb;
        acl->data_received_cb = acl_data_received_cb;
        /* TODO: set the other signal handlers too */
        if (!params) params = default_connect_params(hci);
        bte_acl_connect(acl, params);
    }

    if (UNLIKELY(!acl_l2cap_add_client(L(acl), l2cap))) {
        bte_l2cap_unref(l2cap);
        callback(NULL, NULL, userdata);
        return;
    }

    l2cap->userdata = userdata;
    l2cap->psm = psm;
    l2cap->last_async_cmd_data.connect.client_cb = callback;
}


BteL2cap *bte_l2cap_ref(BteL2cap *l2cap)
{
    atomic_fetch_add(&l2cap->ref_count, 1);
    return l2cap;
}

void bte_l2cap_unref(BteL2cap *l2cap)
{
    if (atomic_fetch_sub(&l2cap->ref_count, 1) == 1) {
        bte_l2cap_free(l2cap);
    }
}

void bte_l2cap_configure(
    BteL2cap *l2cap, const BteL2capConfigureParams *params,
    BteL2capConfigureCb callback, void *userdata)
{
    if (UNLIKELY(l2cap->expected_response_count > 0)) {
        /* Must first wait for a reply to the last command */
        BteL2capConfigureReply reply;
        reply.rejected_mask = 0xffffffff;
        callback(l2cap, &reply, userdata);
        return;
    }

    L2capConfigureData conf = { 0, 0, *params };
    bool ok = l2cap_config_send(l2cap, &conf, 0);
    if (LIKELY(ok)) {
        l2cap->last_async_cmd_data.configure.client_cb = callback;
        l2cap->last_async_cmd_data.configure.userdata = userdata;
    } else {
        BteL2capConfigureReply reply;
        reply.rejected_mask = 0xffffffff;
        callback(l2cap, &reply, userdata);
    }
}

void bte_l2cap_on_configure(
    BteL2cap *l2cap, BteL2capOnConfigureCb callback, void *userdata)
{
    l2cap->configure_cb = callback;
    l2cap->configure_userdata = userdata;
}

void bte_l2cap_set_configure_reply(BteL2cap *l2cap,
                                   const BteL2capConfigureReply *reply)
{
    L2capConfigureData *conf = l2cap->configure_req;
    if (UNLIKELY(!conf)) {
        BTE_WARN("%s: missing incoming request!\n", __func__);
        return;
    }

    conf->rejected_mask = reply->rejected_mask;
    BteL2capConfigureParams *out = &conf->params;
    const BteL2capConfigureParams *in = &reply->params;
    out->field_mask = in->field_mask;
    out->mtu = in->mtu;
    out->flush_timeout = in->flush_timeout;
    out->frame_check_sequence = in->frame_check_sequence;
    out->max_window_size = in->max_window_size;
    if (in->field_mask & BTE_L2CAP_CONFIG_QOS) {
        if (!out->qos) {
            out->qos = malloc(sizeof(BteL2capConfigQos));
        }
        memcpy((void*)out->qos, in->qos, sizeof(BteL2capConfigQos));
    }
    if (in->field_mask & BTE_L2CAP_CONFIG_RETX_FLOW) {
        if (!out->retx_flow) {
            out->retx_flow = malloc(sizeof(BteL2capConfigRetxFlow));
        }
        memcpy((void*)out->retx_flow, in->retx_flow, sizeof(BteL2capConfigRetxFlow));
    }
    if (in->field_mask & BTE_L2CAP_CONFIG_EXT_FLOW) {
        if (!out->ext_flow) {
            out->ext_flow = malloc(sizeof(BteL2capConfigExtFlow));
        }
        memcpy((void*)out->ext_flow, in->ext_flow, sizeof(BteL2capConfigExtFlow));
    }
}
