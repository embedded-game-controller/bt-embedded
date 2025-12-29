#ifndef BTE_ACL_H
#define BTE_ACL_H

#include "buffer.h"
#include "hci.h"
#include "types.h"

#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BUILDING_BT_EMBEDDED
#error "This is not a public header!"
#endif

typedef struct bte_acl_t BteAcl;

struct bte_acl_t {
    atomic_int ref_count;

    BteHci *hci;
    BteBdAddr address;
    BteConnHandle conn_handle;
    uint8_t encryption_mode;

    BteBuffer *fragmented_message;
    uint16_t fragmented_message_size;
    uint16_t reassembled_message_size;

    void (*connected_cb)(BteAcl *acl, uint8_t status);
    void (*disconnected_cb)(BteAcl *acl, uint8_t reason);
    void (*data_received_cb)(BteAcl *acl, BteBufferReader *reader);
    void (*completed_packets_cb)(BteAcl *acl, int sent_messages);
};

BteAcl *bte_acl_new(BteHci *hci, const BteBdAddr *address,
                    size_t struct_size);
BteAcl *bte_acl_get_for_address(BteHci *hci, const BteBdAddr *address);

BteAcl *bte_acl_ref(BteAcl *acl);
void bte_acl_unref(BteAcl *acl);

/* Upon completion, emits the connected_cb event */
void bte_acl_connect(BteAcl *acl, const BteHciConnectParams *params);
void bte_acl_disconnect(BteAcl *acl);

#define BTE_ACL_BROADCAST_PTP    (uint8_t)0
#define BTE_ACL_BROADCAST_ACTIVE (uint8_t)1
#define BTE_ACL_BROADCAST_PARKED (uint8_t)2

bool bte_acl_create_message(BteAcl *acl, BteBufferWriter *writer,
                            uint16_t size, uint8_t broadcast);

/* Return the number of packets sent; 0 if all have been queued */
int bte_acl_send_message(BteAcl *acl, BteBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif /* BTE_ACL_H */
