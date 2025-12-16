#ifndef BTE_L2CAP_H
#define BTE_L2CAP_H

#include "hci.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t BteL2capChannelId;

#define BTE_L2CAP_CHANNEL_ID_NULL       (BteL2capChannelId)0x0000
#define BTE_L2CAP_CHANNEL_ID_SIGNALLING (BteL2capChannelId)0x0001
#define BTE_L2CAP_CHANNEL_ID_RECEPTION  (BteL2capChannelId)0x0002

typedef uint16_t BteL2capPsm;

#define BTE_L2CAP_PSM_SDP       (BteL2capPsm)0x0001
#define BTE_L2CAP_PSM_RFCOMM    (BteL2capPsm)0x0003
#define BTE_L2CAP_PSM_TEL_CORD  (BteL2capPsm)0x0005
#define BTE_L2CAP_PSM_TCS       (BteL2capPsm)0x0007
#define BTE_L2CAP_PSM_BNEP      (BteL2capPsm)0x000f
#define BTE_L2CAP_PSM_HID_CTRL  (BteL2capPsm)0x0011
#define BTE_L2CAP_PSM_HID_INTR  (BteL2capPsm)0x0013
#define BTE_L2CAP_PSM_HID_UPNP  (BteL2capPsm)0x0015
#define BTE_L2CAP_PSM_HID_AVCTP (BteL2capPsm)0x0017
#define BTE_L2CAP_PSM_HID_AVDTP (BteL2capPsm)0x0019

typedef struct {
    BteL2capChannelId remote_channel_id;
    BteL2capChannelId local_channel_id;
    uint16_t result;
    uint16_t status;
} BteL2capConnectionResponse;

#define BTE_L2CAP_CONN_RESP_RES_OK           (uint16_t)0
#define BTE_L2CAP_CONN_RESP_RES_PENDING      (uint16_t)1
#define BTE_L2CAP_CONN_RESP_RES_ERR_PSM      (uint16_t)2
#define BTE_L2CAP_CONN_RESP_RES_ERR_SECBLOCK (uint16_t)3
#define BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE (uint16_t)4
#define BTE_L2CAP_CONN_RESP_RES_ERR_INV_SCID (uint16_t)6
#define BTE_L2CAP_CONN_RESP_RES_ERR_DUP_SCID (uint16_t)7

#define BTE_L2CAP_CONN_RESP_STATUS_NO_INFO        (uint16_t)0
#define BTE_L2CAP_CONN_RESP_STATUS_AUTHENTICATION (uint16_t)1
#define BTE_L2CAP_CONN_RESP_STATUS_AUTHORIZATION  (uint16_t)2

typedef void (*BteL2capConnectCb)(
    BteL2cap *l2cap, const BteL2capConnectionResponse *reply, void *userdata);

/**
 * \note The callback can be invoked more than once, if \a result is \c
 *       BTE_L2CAP_CONN_RESP_RES_PENDING.
 */
void bte_l2cap_connect(BteHci *hci, const BteBdAddr *address, BteL2capPsm psm,
                       const BteHciConnectParams *params,
                       BteL2capConnectCb callback, void *userdata);

BteL2cap *bte_l2cap_ref(BteL2cap *l2cap);
void bte_l2cap_unref(BteL2cap *l2cap);

void bte_l2cap_set_userdata(BteL2cap *l2cap, void *userdata);
void *bte_l2cap_get_userdata(BteL2cap *l2cap);

BteConnHandle bte_l2cap_get_connection_handle(BteL2cap *l2cap);
BteClient *bte_l2cap_get_client(BteL2cap *l2cap);
BteHci *bte_l2cap_get_hci(BteL2cap *l2cap);

typedef struct {
    uint8_t flags;
    uint8_t service_type;
    uint32_t token_rate;
    uint32_t token_bucket_size;
    uint32_t peak_bandwith;
    uint32_t access_latency;
    uint32_t delay_variation;
} BTE_PACKED BteL2capConfigQos;

typedef struct {
    uint8_t mode;
    uint8_t tx_window_size;
    uint8_t max_transmit;
    uint16_t retx_timeout;
    uint16_t monitor_timeout;
    uint16_t max_pdu_size;
} BTE_PACKED BteL2capConfigRetxFlow;

typedef struct {
    uint8_t identifier;
    uint8_t service_type;
    uint16_t max_sdu_size;
    uint32_t sdu_inter_time;
    uint32_t access_latency;
    uint32_t flush_timeout;
} BTE_PACKED BteL2capConfigExtFlow;

typedef struct {
    uint32_t field_mask;
    uint16_t mtu;
    uint16_t flush_timeout;
    uint8_t frame_check_sequence;
    uint16_t max_window_size;
    const BteL2capConfigQos *qos;
    const BteL2capConfigRetxFlow *retx_flow;
    const BteL2capConfigExtFlow *ext_flow;
} BteL2capConfigureParams;

#define BTE_L2CAP_CONFIG_MTU             (1 << 0)
#define BTE_L2CAP_CONFIG_FLUSH_TIMEOUT   (1 << 1)
#define BTE_L2CAP_CONFIG_QOS             (1 << 2)
#define BTE_L2CAP_CONFIG_RETX_FLOW       (1 << 3)
#define BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ (1 << 4)
#define BTE_L2CAP_CONFIG_EXT_FLOW        (1 << 5)
#define BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE (1 << 6)

typedef struct {
    uint32_t rejected_mask;
    uint32_t unknown_mask; /* only used in incoming responses */
    BteL2capConfigureParams params;
} BteL2capConfigureReply;

typedef void (*BteL2capConfigureCb)(
    BteL2cap *l2cap, const BteL2capConfigureReply *reply, void *userdata);

void bte_l2cap_configure(
    BteL2cap *l2cap, const BteL2capConfigureParams *params,
    BteL2capConfigureCb callback, void *userdata);

typedef void (*BteL2capOnConfigureCb)(
    BteL2cap *l2cap, const BteL2capConfigureParams *params, void *userdata);

void bte_l2cap_on_configure(
    BteL2cap *l2cap, BteL2capOnConfigureCb callback, void *userdata);
/* Can only be called from within a BteL2capOnConfigureCb callback */
void bte_l2cap_set_configure_reply(BteL2cap *l2cap,
                                   const BteL2capConfigureReply *reply);

#ifdef __cplusplus
}
#endif

#endif /* BTE_L2CAP_H */
