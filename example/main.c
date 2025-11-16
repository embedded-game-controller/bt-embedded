#include "terminal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bt-embedded/bte.h"
#include "bt-embedded/client.h"
#include "bt-embedded/hci.h"

static void inquiry_status_cb(BteHci *hci, const BteHciReply *reply, void *)
{
    printf("Inquiry issued, status = %d\n", reply->status);
}

static void inquiry_cb(BteHci *hci, const BteHciInquiryReply *reply, void *)
{
    printf("Inquiry done, status = %d\n", reply->status);
    if (reply->status != 0) return;

    printf("Results: %d, ptr %p\n", reply->num_responses, reply->responses);
    for (int i = 0; i < reply->num_responses; i++) {
        const uint8_t *b = reply->responses[i].address.bytes;
        printf(" - %02x:%02x:%02x:%02x:%02x:%02x\n", b[0], b[1], b[2], b[3], b[4], b[5]);
    }
}

static void initialized_cb(BteHci *hci, bool success, void *)
{
    printf("Initialized, OK = %d\n", success);
    printf("ACL MTU=%d, max packets=%d\n",
           bte_hci_get_acl_mtu(hci),
           bte_hci_get_acl_max_packets(hci));
    //bte_hci_inquiry(hci, BTE_LAP_GIAC, 3, 0, inquiry_status_cb, inquiry_cb);
    bte_hci_periodic_inquiry(hci, 4, 5, BTE_LAP_GIAC, 3, 0, inquiry_status_cb, inquiry_cb);
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
        bte_wait_events(1000000);
    }

    bte_client_unref(client);
    return EXIT_SUCCESS;
}
