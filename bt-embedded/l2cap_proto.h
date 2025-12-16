#ifndef BTE_L2CAP_PROTO_H
#define BTE_L2CAP_PROTO_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BUILDING_BT_EMBEDDED
#error "This is not a public header!"
#endif

#define L2CAP_HDR_LEN 4
#define L2CAP_SIGNAL_HDR_LEN 4

#define L2CAP_CONFIG_REQ_HDR_LEN 4
#define L2CAP_CONFIG_RSP_HDR_LEN 6

/* Signals */
#define L2CAP_SIGNAL_CMD_REJ     0x01
#define L2CAP_SIGNAL_CONN_REQ    0x02
#define L2CAP_SIGNAL_CONN_RSP    0x03
#define L2CAP_SIGNAL_CONFIG_REQ  0x04
#define L2CAP_SIGNAL_CONFIG_RSP  0x05
#define L2CAP_SIGNAL_DISCONN_REQ 0x06
#define L2CAP_SIGNAL_DISCONN_RSP 0x07
#define L2CAP_SIGNAL_ECHO_REQ    0x08
#define L2CAP_SIGNAL_ECHO_RSP    0x09
#define L2CAP_SIGNAL_INFO_REQ    0x0A
#define L2CAP_SIGNAL_INFO_RSP    0x0B

#define L2CAP_MTU_MIN     48
#define L2CAP_MTU_DEFAULT 672

#define L2CAP_CONFIG_FLAG_CONTINUATION (1 << 0)

#define L2CAP_CONFIG_RES_OK          (uint16_t)0
#define L2CAP_CONFIG_RES_ERR_PARAMS  (uint16_t)1
#define L2CAP_CONFIG_RES_ERR_REJ     (uint16_t)2
#define L2CAP_CONFIG_RES_ERR_UNKNOWN (uint16_t)3

#define L2CAP_CONFIG_MTU             0x01
#define L2CAP_CONFIG_FLUSH_TIMEOUT   0x02
#define L2CAP_CONFIG_QOS             0x03
#define L2CAP_CONFIG_RETX_FLOW       0x04
#define L2CAP_CONFIG_FRAME_CHECK_SEQ 0x05
#define L2CAP_CONFIG_EXT_FLOW        0x06
#define L2CAP_CONFIG_MAX_WINDOW_SIZE 0x07
#define L2CAP_CONFIG_HINT            0x80

#define L2CAP_CONFIG_MTU_LEN             2
#define L2CAP_CONFIG_FLUSH_TIMEOUT_LEN   2
#define L2CAP_CONFIG_QOS_LEN             22
#define L2CAP_CONFIG_RETX_FLOW_LEN       9
#define L2CAP_CONFIG_FRAME_CHECK_SEQ_LEN 1
#define L2CAP_CONFIG_EXT_FLOW_LEN        16
#define L2CAP_CONFIG_MAX_WINDOW_SIZE_LEN 2

#ifdef __cplusplus
}
#endif

#endif /* BTE_L2CAP_PROTO_H */
