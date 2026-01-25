#ifndef BTE_L2CAP_PRIV_H
#define BTE_L2CAP_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "l2cap.h"

#include "acl.h"

/* Maximum number of l2cap connections over the same ACL link */
#define BTE_ACL_MAX_CLIENTS 4

typedef struct bte_acl_l2cap_t {
    BteAcl acl;

    BteL2cap *clients[BTE_ACL_MAX_CLIENTS];
} BteAclL2cap;

bool acl_l2cap_create_cmd(BteAclL2cap *acl_l2cap,
                          BteBufferWriter *writer,
                          uint8_t code, uint8_t id, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /*BTE_L2CAP_PRIV_H */
