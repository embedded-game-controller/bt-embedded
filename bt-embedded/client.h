#ifndef BTE_CLIENT_H
#define BTE_CLIENT_H

#include "types.h"

BteClient *bte_client_new(void);
BteClient *bte_client_ref(BteClient *client);
void bte_client_unref(BteClient *client);

void bte_client_set_userdata(BteClient *client, void *userdata);
void *bte_client_get_userdata(BteClient *client);

#endif /* BTE_CLIENT_H */
