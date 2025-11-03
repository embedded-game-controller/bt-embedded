#ifndef BTE_TYPES_H
#define BTE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* Common **public** types used by the library and its clients */

typedef struct bte_buffer_t BteBuffer;
typedef struct bte_client_t BteClient;
typedef struct bte_hci_t BteHci;

typedef struct {
    uint8_t bytes[6];
} BteBdAddr;

typedef struct {
    uint8_t bytes[3];
} BteClassOfDevice;

#endif
