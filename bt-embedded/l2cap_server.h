#ifndef BTE_L2CAP_SERVER_H
#define BTE_L2CAP_SERVER_H

#include "l2cap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bte_l2cap_server_t BteL2capServer;

BteL2capServer *bte_l2cap_server_new(BteClient *client, BteL2capPsm psm);

BteL2capServer *bte_l2cap_server_ref(BteL2capServer *l2cap_server);
void bte_l2cap_server_unref(BteL2capServer *l2cap_server);

BteClient *bte_l2cap_server_get_client(BteL2capServer *l2cap_server);
BteHci *bte_l2cap_server_get_hci(BteL2capServer *l2cap_server);

void bte_l2cap_server_set_needs_auth(BteL2capServer *l2cap_server,
                                     bool needs_auth);
void bte_l2cap_server_set_role(BteL2capServer *l2cap_server, BteRole role);

typedef void (*BteL2capServerConnectedCb)(
    BteL2capServer *l2cap_server, BteL2cap *l2cap, void *userdata);

void bte_l2cap_server_on_connected(
    BteL2capServer *l2cap_server, BteL2capServerConnectedCb connected_cb,
    void *userdata);

/**
 * Return true if the connection should be accepted, false otherwise.
 */
typedef bool (*BteL2capServerConnectionRequestCb)(
    BteL2capServer *l2cap_server, const BteBdAddr *address,
    const BteClassOfDevice *cod, void *userdata);

/**
 * Call this function if you want to control which incoming connections
 * should be accepted. By default, all connections are accepted.
 *
 * \param callback The function that will be called when an incoming
 *        connection is requested.
 */
void bte_l2cap_server_on_connection_request(
    BteL2capServer *l2cap_server, BteL2capServerConnectionRequestCb callback,
    void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* BTE_L2CAP_SERVER_H */
