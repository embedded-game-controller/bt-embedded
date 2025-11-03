#include "terminal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bt-embedded/bte.h"
#include "bt-embedded/client.h"
#include "bt-embedded/hci.h"

static void initialized_cb(BteHci *hci, bool success, void *)
{
    printf("Initialized, OK = %d\n", success);
    printf("ACL MTU=%d, max packets=%d\n",
           bte_hci_get_acl_mtu(hci),
           bte_hci_get_acl_max_packets(hci));
}

int main(int argc, char **argv)
{
    quit_requested = false;

    /* Some platforms need to perform some more steps before having the console
     * output setup. */
    terminal_init();

    printf("Initializing...\n");
    BteClient *client = bte_client_new();
    BteHci *hci = bte_hci_get(client);
    printf("client: %p, hci = %p\n", client, hci);
    bte_hci_on_initialized(hci, initialized_cb);

    while (!quit_requested) {
        bte_wait_events();
    }

    bte_client_unref(client);
    return EXIT_SUCCESS;
}
