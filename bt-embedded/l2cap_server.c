#include "l2cap_server.h"

#include "acl.h"
#include "client.h"
#include "hci.h"
#include "internals.h"
#include "logging.h"

struct bte_l2cap_server_t {
    atomic_int ref_count;
    BteHci *hci;
    BteL2capPsm psm;
    BteL2capServerConnectedCb connected_cb;
    void *userdata;
    struct bte_l2cap_server_t *next;
};

static BteL2capServer *s_servers = NULL;
BteL2capConnectionRequestCb _bte_l2cap_handle_connection_req;

static BteL2capServer *server_for_psm(BteL2capPsm psm)
{
    for (BteL2capServer *s = s_servers; s != NULL; s = s->next) {
        if (s->psm == psm) return s;
    }
    return NULL;
}

static void rely_to_client(BteL2capServer *l2cap_server, BteL2cap *l2cap)
{
    l2cap_server->connected_cb(l2cap_server, l2cap, l2cap_server->userdata);
    /* The client should have taken its own reference */
    bte_l2cap_unref(l2cap);
}

static void on_l2cap_state_changed(BteL2cap *l2cap, BteL2capState state,
                                   void *userdata)
{
    if (state == BTE_L2CAP_WAIT_CONFIG) {
        /* We are going to rely the l2cap object to the client. Make sure all
         * our callbacks have been disconnected */
        bte_l2cap_on_state_changed(l2cap, NULL);
        BteL2capServer *server = server_for_psm(l2cap->psm);
        rely_to_client(server, l2cap);
    }
}

static BteL2cap *l2cap_server_handle_connection_req(
    BteAcl *acl, BteL2capPsm psm, BteL2capChannelId channel_id)
{
    BteL2capServer *server = server_for_psm(psm);
    if (UNLIKELY(!server)) return NULL;

    BteL2cap *l2cap = _bte_l2cap_new_connected(acl, psm, channel_id);
    bte_l2cap_on_state_changed(l2cap, on_l2cap_state_changed);
    return l2cap;
}

static void hci_accept_connection_cb(BteHci *hci,
                                     const BteHciAcceptConnectionReply *reply,
                                     void *userdata)
{
    if (reply->status != 0) return;

    _bte_l2cap_acl_new_connected(hci, reply);
    /* TODO: do we need to store this object? */
}

static bool hci_connection_request_cb(BteHci *hci,
                                      const BteBdAddr *address,
                                      const BteClassOfDevice *cod,
                                      BteLinkType link_type,
                                      void *userdata)
{
    if (link_type != BTE_LINK_TYPE_ACL) return false;

    /* We accept all requests; if the peer will attempt to request a PSM that
     * we don't support, the connection will get closed at that point. */
    bte_hci_accept_connection(hci, address, BTE_HCI_ROLE_SLAVE, NULL,
                              hci_accept_connection_cb, NULL);
    return true;
}

static void bte_l2cap_server_free(BteL2capServer *l2cap_server)
{
    BteHci *hci = l2cap_server->hci;

    BteL2capServer **p_server = &s_servers;
    while (*p_server != l2cap_server) {
        p_server = &((*p_server)->next);
    }
    *p_server = l2cap_server->next;
    bte_free(l2cap_server);

    if (!s_servers) {
        bte_hci_write_scan_enable(hci, BTE_HCI_SCAN_ENABLE_OFF, NULL, NULL);
    }
    bte_client_unref(bte_hci_get_client(hci));
}

BteL2capServer *bte_l2cap_server_new(BteClient *client, BteL2capPsm psm)
{
    _bte_l2cap_handle_connection_req = l2cap_server_handle_connection_req;
    BteL2capServer *l2cap_server = bte_malloc(sizeof(BteL2capServer));
    memset(l2cap_server, 0, sizeof(BteL2capServer));
    l2cap_server->ref_count = 1;
    l2cap_server->hci = bte_hci_get(bte_client_ref(client));
    l2cap_server->psm = psm;
    l2cap_server->next = s_servers;
    s_servers = l2cap_server;
    return l2cap_server;
}

BteL2capServer *bte_l2cap_server_ref(BteL2capServer *l2cap_server)
{
    atomic_fetch_add(&l2cap_server->ref_count, 1);
    return l2cap_server;
}

void bte_l2cap_server_unref(BteL2capServer *l2cap_server)
{
    if (atomic_fetch_sub(&l2cap_server->ref_count, 1) == 1) {
        bte_l2cap_server_free(l2cap_server);
    }
}

BteClient *bte_l2cap_server_get_client(BteL2capServer *l2cap_server)
{
    return bte_hci_get_client(l2cap_server->hci);
}

BteHci *bte_l2cap_server_get_hci(BteL2capServer *l2cap_server)
{
    return l2cap_server->hci;
}

void bte_l2cap_server_on_connected(
    BteL2capServer *l2cap_server, BteL2capServerConnectedCb connected_cb,
    void *userdata)
{
    l2cap_server->connected_cb = connected_cb;
    l2cap_server->userdata = userdata;

    bte_hci_on_connection_request(l2cap_server->hci,
                                  hci_connection_request_cb);
    bte_hci_write_scan_enable(l2cap_server->hci,
                              BTE_HCI_SCAN_ENABLE_PAGE, NULL, NULL);
}
