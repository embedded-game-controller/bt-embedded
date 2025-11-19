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

static inline void print_hex(const uint8_t *b, int n, char sep)
{
    for (int i = 0; i < n; i++) {
        printf("%02x%c", b[i], (i == n - 1) ? ' ' : sep);
    }
}

static inline void print_addr(const BteBdAddr *address)
{
    print_hex(address->bytes, 6, ':');
}

static inline void print_key(const BteLinkKey *key)
{
    print_hex(key->bytes, sizeof(*key), '-');
}

static void inquiry_cb(BteHci *hci, const BteHciInquiryReply *reply, void *)
{
    printf("Inquiry done, status = %d\n", reply->status);
    if (reply->status != 0) return;

    printf("Results: %d\n", reply->num_responses);
    for (int i = 0; i < reply->num_responses; i++) {
        const uint8_t *b = reply->responses[i].address.bytes;
        printf(" - %02x:%02x:%02x:%02x:%02x:%02x\n", b[0], b[1], b[2], b[3],
               b[4], b[5]);
    }
    static int count = 0;
    if (count++ > 3) {
        bte_hci_exit_periodic_inquiry(hci, NULL);
    }
    // bte_hci_inquiry(hci, BTE_LAP_GIAC, 3, 0, inquiry_status_cb, inquiry_cb);
}

static void read_bd_addr_cb(BteHci *hci, const BteHciReadBdAddrReply *reply,
                            void *userdata)
{
    static int count = 0;
    const uint8_t *b = reply->address.bytes;
    printf("%s: (%d), %02x:%02x:%02x:%02x:%02x:%02x\n", __func__, count, b[0],
           b[1], b[2], b[3], b[4], b[5]);
    count++;
    // bte_hci_read_bd_addr(hci, read_bd_addr_cb);
}

static void read_stored_link_key_cb(BteHci *hci,
                                    const BteHciReadStoredLinkKeyReply *reply,
                                    void *userdata)
{
    printf("(status=%02x) %d keys out of %d\n", reply->status, reply->num_keys,
           reply->max_keys);
    for (int i = 0; i < reply->num_keys; i++) {
        print_addr(&reply->stored_keys[i].address);
        printf("\n  key:");
        print_key(&reply->stored_keys[i].key);
        printf("\n");
    }
}

static void initialized_cb(BteHci *hci, bool success, void *)
{
    printf("Initialized, OK = %d\n", success);
    printf("ACL MTU=%d, max packets=%d\n",
           bte_hci_get_acl_mtu(hci),
           bte_hci_get_acl_max_packets(hci));
    // bte_hci_read_bd_addr(hci, read_bd_addr_cb);
    // bte_hci_periodic_inquiry(hci, 4, 5, BTE_LAP_GIAC, 3, 0,
    // inquiry_status_cb, inquiry_cb);
    bte_hci_read_stored_link_key(hci, NULL, read_stored_link_key_cb);
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
