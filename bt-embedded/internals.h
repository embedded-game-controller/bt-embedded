#ifndef BTE_INTERNALS_H
#define BTE_INTERNALS_H

#include "data_matcher.h"
#include "hci.h"
#include "types.h"
#include "utils.h"

#include <stdatomic.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
typedef union bte_hci_command_cb_u BteHciCommandCbUnion;

typedef void (*BteHciCommandCb)(BteHci *hci, BteBuffer *buffer,
                                void *client_cb);
typedef void (*BteHciCommandStatusCb)(BteHci *hci, uint8_t status,
                                      BteHciPendingCommand *pc);

typedef struct bte_hci_event_handler_t BteHciEventHandler;
typedef void (*BteHciEventHandlerCb)(BteBuffer *buffer, void *cb_data);

typedef struct bte_hci_dev_t {
    BteHciInitStatus init_status;
    BteHciInfo info_flags;

    uint16_t num_pending_commands;
    struct bte_hci_pending_command_t {
        /* When a result is received, we will look at the opcode (and possibly
         * other data) to deliver the reply to the correct client */
        BteDataMatcher matcher;
        BteHci *hci;
        union bte_hci_command_cb_u {
            struct bte_hci_cmd_complete_t {
                /* This callback is used in sync commands to parse the buffer
                 * into a structured reply for the client. */
                BteHciCommandCb complete;
                void *client_cb;
            } cmd_complete;
            struct bte_hci_cmd_status_t {
                /* This callback is used to process the Command Status event:
                 * it should install any needed event listeners for the actual
                 * command complete event. */
                BteHciCommandStatusCb status;
                BteHciDoneCb client_cb;
            } cmd_status;
            struct bte_hci_event_conn_complete_t {
                BteHciCreateConnectionCb client_cb;
            } event_conn_complete;
            struct bte_hci_event_auth_complete_t {
                BteHciAuthRequestedCb client_cb;
            } event_auth_complete;
            struct bte_hci_event_mode_change_t {
                BteHciModeChangeCb client_cb;
            } event_mode_change;
        } command_cb;
    } pending_commands[BTE_HCI_MAX_PENDING_COMMANDS];

    /* Storage for temporary data, only valid since issuing an asynchronous
     * command till the time that its corresponding command status event has
     * been received. */
    union _bte_hci_last_async_cmd_data_u {
        struct _bte_hci_tmpdata_create_connection_t {
            BteHciCreateConnectionCb client_cb;
            BteBdAddr address;
        } create_connection;
        struct _bte_hci_tmpdata_auth_requested_t {
            BteHciAuthRequestedCb client_cb;
            BteHciConnHandle conn_handle;
        } auth_requested;
    } last_async_cmd_data;

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

    /* Ongoing reading of stored link keys */
    struct bte_hci_stored_keys_t {
        uint8_t num_responses;
        BteHciStoredLinkKey *responses;
    } stored_keys;

    /* We use the 0-index element for vendor-specific events */
    struct bte_hci_event_handler_t {
        BteHciEventHandlerCb handler_cb;
        void *cb_data;
    } event_handlers[BTE_HCI_EVENT_LAST];
} BteHciDev;

struct bte_client_t {
    atomic_int ref_count;
    void *userdata;

    struct bte_hci_t {
        BteInitializedCb initialized_cb;

        BteHciInquiryCb inquiry_cb;
        BteHciConnectionRequestCb connection_request_cb;
        BteHciLinkKeyRequestCb link_key_request_cb;
        BteHciPinCodeRequestCb pin_code_request_cb;

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
void _bte_hci_dispose(BteHci *hci);

typedef bool (*BteHciForeachHciClientCb)(BteHci *hci, void *cb_data);
bool _bte_hci_dev_foreach_hci_client(BteHciForeachHciClientCb callback,
                                     void *cb_data);

/* Called by the driver once the initialization is complete */
void _bte_hci_dev_set_status(BteHciInitStatus status);

/* Called by the platform backend */
int _bte_hci_dev_handle_event(BteBuffer *buf);
int _bte_hci_dev_handle_data(BteBuffer *buf);

/* Called by the HCI layer */
BteHciPendingCommand *_bte_hci_dev_alloc_command(
    const BteDataMatcher *matcher);
BteHciPendingCommand *_bte_hci_dev_get_pending_command(
    const BteDataMatcher *matcher);

BteBuffer *_bte_hci_dev_add_command_no_reply(uint16_t ocf, uint8_t ogf,
                                             uint8_t len);
BteBuffer *_bte_hci_dev_add_command(BteHci *hci, uint16_t ocf,
                                    uint8_t ogf, uint8_t len,
                                    uint8_t reply_event,
                                    const BteHciCommandCbUnion *command_cb);
#ifndef __cplusplus
static inline BteBuffer *
_bte_hci_dev_add_pending_command(BteHci *hci, uint16_t ocf,
                                 uint8_t ogf, uint8_t len,
                                 BteHciCommandCb command_cb,
                                 void *client_cb)
{
    BteHciCommandCbUnion cmd = {
        .cmd_complete = { command_cb, client_cb }
    };
    return _bte_hci_dev_add_command(hci, ocf, ogf, len,
                                    HCI_COMMAND_COMPLETE, &cmd);
}

static inline BteBuffer *
_bte_hci_dev_add_pending_async_command(BteHci *hci, uint16_t ocf,
                                       uint8_t ogf, uint8_t len,
                                       BteHciCommandStatusCb command_cb,
                                       void *client_cb)
{
    BteHciCommandCbUnion cmd = {
        .cmd_status = { command_cb, client_cb }
    };
    return _bte_hci_dev_add_command(hci, ocf, ogf, len,
                                    HCI_COMMAND_STATUS, &cmd);
}
#endif // __cplusplus

BteHciPendingCommand *_bte_hci_dev_find_pending_command(
    const BteBuffer *buffer);
BteHciPendingCommand *_bte_hci_dev_find_pending_command_raw(
    const void *buffer, size_t len);
void _bte_hci_dev_free_command(BteHciPendingCommand *cmd);
int _bte_hci_send_command(BteBuffer *buffer);

void _bte_hci_dev_install_event_handler(uint8_t event_code,
                                        BteHciEventHandlerCb handler_cb,
                                        void *cb_data);
BteHciEventHandler *_bte_hci_dev_handler_for_event(uint8_t event_code);

void _bte_hci_dev_inquiry_cleanup(void);
void _bte_hci_dev_stored_keys_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* BTE_INTERNALS_H */
