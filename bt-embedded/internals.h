#ifndef BTE_INTERNALS_H
#define BTE_INTERNALS_H

#include "hci.h"
#include "types.h"
#include "utils.h"

#include <stdatomic.h>
#include <stddef.h>

#ifndef BUILDING_BT_EMBEDDED
#error "This is not a public header!"
#endif

#define BTE_HCI_MAX_PENDING_COMMANDS 8
/* Note that the driver typically creates a client to setup the device, so this
 * must be 2 at the very least. */
#define BTE_HCI_MAX_CLIENTS 4

/* This is the last (in the sense of its numerical value) event that we
 * support. It could be decreased is we are sure that our clients don't need
 * support a certain event, and then we could free up some bytes in the event
 * handler array. */
#ifndef BTE_HCI_EVENT_LAST
#  define BTE_HCI_EVENT_LAST HCI_REMOTE_HOST_FEATURES_NOTIFY
#endif

typedef enum {
    BTE_HCI_INIT_STATUS_UNINITIALIZED = 0,
    BTE_HCI_INIT_STATUS_INITIALIZING,
    BTE_HCI_INIT_STATUS_INITIALIZED,
    BTE_HCI_INIT_STATUS_FAILED,
} BteHciInitStatus;

typedef enum {
    BTE_HCI_INFO_GOT_FEATURES = 1 << 0,
    BTE_HCI_INFO_GOT_BUFFER_SIZE = 1 << 1,
} BteHciInfo;

typedef struct bte_hci_pending_command_t BteHciPendingCommand;
typedef void (*BteHciCommandCb)(BteHci *hci, BteBuffer *buffer,
                                void *client_cb);

typedef struct bte_hci_event_handler_t BteHciEventHandler;
typedef void (*BteHciEventHandlerCb)(BteBuffer *buffer, void *cb_data);

typedef struct bte_hci_dev_t {
    BteHciInitStatus init_status;
    BteHciInfo info_flags;

    uint16_t num_pending_commands;
    struct bte_hci_pending_command_t {
        /* When a result is received, we will look at the opcode (and possibly
         * other data) to deliver the reply to the correct client */
        BteBuffer *buffer;
        BteHciCommandCb command_cb;
        BteHci *hci;
        void *client_cb;
        BteHciDoneCb client_status_cb; /* Only for async commands */
    } pending_commands[BTE_HCI_MAX_PENDING_COMMANDS];

    BteClient *clients[BTE_HCI_MAX_CLIENTS];

    BteBdAddr address;
    BteHciSupportedFeatures supported_features;
    atomic_int num_packets;
    uint16_t acl_mtu;
    uint8_t sco_mtu;
    uint16_t acl_max_packets;
    uint16_t sco_max_packets;

    /* Ongoing inquiry data */
    struct bte_hci_inquiry_data_t {
        uint8_t num_responses;
        BteHciInquiryResponse *responses;
    } inquiry;

    /* We use the 0-index element for vendor-specific events */
    struct bte_hci_event_handler_t {
        BteHciEventHandlerCb handler_cb;
        void *cb_data; /* TODO: we can probably change this to be a BteHci* */
    } event_handlers[BTE_HCI_EVENT_LAST];
} BteHciDev;

struct bte_client_t {
    atomic_int ref_count;
    void *userdata;

    struct bte_hci_t {
        BteInitializedCb initialized_cb;

        BteHciInquiryCb inquiry_cb; /* Used for periodic inquiries */
        /* Should we ever start supporting more than one HCI device, we should
         * store a pointer to the HCI device here. AS of now, we have a single
         * HCI device, accessible under the _bte_hci_dev global variable. */
    } hci;
};

static inline BteClient *hci_client(BteHci *hci)
{
    return (BteClient *)((uint8_t *)hci - offsetof(BteClient, hci));
}

static inline void *hci_userdata(BteHci *hci)
{
    return hci_client(hci)->userdata;
}

extern BteHciDev _bte_hci_dev;

int _bte_hci_dev_init(void);
bool _bte_hci_dev_add_client(BteClient *client);
void _bte_hci_dev_remove_client(BteClient *client);

/* Called by the driver once the initialization is complete */
void _bte_hci_dev_set_status(BteHciInitStatus status);

/* Called by the platform backend */
int _bte_hci_dev_handle_event(BteBuffer *buf);
int _bte_hci_dev_handle_data(BteBuffer *buf);

/* Called by the HCI layer */
BteBuffer *_bte_hci_dev_add_pending_async_command(BteHci *hci, uint16_t ocf,
                                                  uint8_t ogf, uint8_t len,
                                                  BteHciCommandCb command_cb,
                                                  BteHciDoneCb status_cb,
                                                  void *client_cb);
BteBuffer *_bte_hci_dev_add_pending_command(BteHci *hci, uint16_t ocf,
                                            uint8_t ogf, uint8_t len,
                                            BteHciCommandCb command_cb,
                                            void *client_cb);
int _bte_hci_send_command(BteBuffer *buffer);

void _bte_hci_dev_install_event_handler(uint8_t event_code,
                                        BteHciEventHandlerCb handler_cb,
                                        void *cb_data);

void _bte_hci_dev_inquiry_cleanup(void);

#endif /* BTE_INTERNALS_H */
