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

#ifdef __cplusplus
}
#endif

#endif
