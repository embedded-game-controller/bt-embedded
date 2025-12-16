#ifndef BTE_TYPES_H
#define BTE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Common **public** types used by the library and its clients */

#define BTE_PACKED __attribute__((packed))

typedef struct bte_buffer_t BteBuffer;
typedef struct bte_client_t BteClient;
typedef struct bte_hci_t BteHci;
typedef struct bte_l2cap_t BteL2cap;

typedef struct {
    uint8_t bytes[6];
} BteBdAddr;

typedef struct {
    uint8_t bytes[3];
} BteClassOfDevice;

typedef struct {
    uint8_t bytes[16];
} BteLinkKey;

typedef uint32_t BteLap;

typedef uint16_t BtePacketType;

#define BTE_PACKET_TYPE_DM1 (BtePacketType)0x0008
#define BTE_PACKET_TYPE_DH1 (BtePacketType)0x0010
#define BTE_PACKET_TYPE_DM3 (BtePacketType)0x0400
#define BTE_PACKET_TYPE_DH3 (BtePacketType)0x0800
#define BTE_PACKET_TYPE_DH5 (BtePacketType)0x4000
#define BTE_PACKET_TYPE_DM5 (BtePacketType)0x8000

typedef uint16_t BteConnHandle;

/* The connection handle is 12 bits, so this value is certainly invalid */
#define BTE_CONN_HANDLE_INVALID (BteConnHandle)0xffff

#ifdef __cplusplus
}
#endif

#endif
