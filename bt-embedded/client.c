#include "client.h"

#include "hci.h"
#include "internals.h"
#include "logging.h"

#include <stdlib.h>
#include <string.h>

static void bte_client_free(BteClient *client)
{
    _bte_hci_dev_remove_client(client);
    free(client);
}

BteClient *bte_client_new(void)
{
    if (UNLIKELY(_bte_hci_dev_init() < 0)) {
        return NULL;
    }

    BteClient *client = malloc(sizeof(BteClient));
    if (UNLIKELY(!client)) return NULL;

    memset(client, 0, sizeof(*client));
    client->ref_count = 1;
    if (UNLIKELY(!_bte_hci_dev_add_client(client))) {
        bte_client_unref(client);
        return NULL;
    }
    return client;
}

BteClient *bte_client_ref(BteClient *client)
{
    atomic_fetch_add(&client->ref_count, 1);
    return client;
}

void bte_client_unref(BteClient *client)
{
    if (atomic_fetch_sub(&client->ref_count, 1) == 1) {
        bte_client_free(client);
    }
}

void bte_client_set_userdata(BteClient *client, void *userdata)
{
    client->userdata = userdata;
}

void *bte_client_get_userdata(BteClient *client)
{
    return client->userdata;
}
