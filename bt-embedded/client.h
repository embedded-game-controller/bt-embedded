#ifndef BTE_CLIENT_H
#define BTE_CLIENT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

BteClient *bte_client_new(void);
BteClient *bte_client_ref(BteClient *client);
void bte_client_unref(BteClient *client);

void bte_client_set_userdata(BteClient *client, void *userdata);
void *bte_client_get_userdata(BteClient *client);

#ifdef __cplusplus
}
#endif

#endif /* BTE_CLIENT_H */
