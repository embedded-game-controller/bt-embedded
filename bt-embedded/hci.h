#ifndef BTE_HCI_H
#define BTE_HCI_H

#include "hci_proto.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t BteHciEventMask;

BteHci *bte_hci_get(BteClient *client);

BteClient *bte_hci_get_client(BteHci *hci);

typedef void (*BteInitializedCb)(BteHci *hci, bool success, void *userdata);
void bte_hci_on_initialized(BteHci *hci, BteInitializedCb callback);

typedef uint64_t BteHciSupportedFeatures;
BteHciSupportedFeatures bte_hci_get_supported_features(BteHci *hci);

uint16_t bte_hci_get_acl_mtu(BteHci *hci);
uint8_t bte_hci_get_sco_mtu(BteHci *hci);
uint16_t bte_hci_get_acl_max_packets(BteHci *hci);
uint16_t bte_hci_get_sco_max_packets(BteHci *hci);

/* All command replies start with this struct */
typedef struct {
    uint8_t status;
} BteHciReply;

/* Used by all functions which don't have additional return parameters */
typedef void (*BteHciDoneCb)(BteHci *hci, const BteHciReply *reply,
                             void *userdata);

void bte_hci_nop(BteHci *hci, BteHciDoneCb callback);

/* Link control commands */

typedef struct {
    BteBdAddr address;
    uint8_t page_scan_rep_mode;
    uint8_t page_scan_period_mode;
    uint8_t reserved;
    BteClassOfDevice class_of_device;
    uint16_t clock_offset;
} BTE_PACKED BteHciInquiryResponse;

typedef struct {
    uint8_t status;
    uint8_t num_responses;
    const BteHciInquiryResponse *responses;
} BteHciInquiryReply;

typedef void (*BteHciInquiryCb)(BteHci *hci,
                                const BteHciInquiryReply *reply,
                                void *userdata);
void bte_hci_inquiry(BteHci *hci, uint32_t lap, uint8_t len, uint8_t max_resp,
                     BteHciDoneCb status_cb,
                     BteHciInquiryCb callback);
void bte_hci_inquiry_cancel(BteHci *hci, BteHciDoneCb callback);
void bte_hci_periodic_inquiry(BteHci *hci,
                              uint16_t min_period, uint16_t max_period,
                              uint32_t lap, uint8_t len, uint8_t max_resp,
                              BteHciDoneCb status_cb,
                              BteHciInquiryCb callback);
void bte_hci_exit_periodic_inquiry(BteHci *hci, BteHciDoneCb callback);

/* Return true if this client will handle the event */
typedef bool (*BteHciLinkKeyRequestCb)(BteHci *hci,
                                       const BteBdAddr *address,
                                       void *userdata);
void bte_hci_on_link_key_request(BteHci *hci, BteHciLinkKeyRequestCb callback);

typedef struct {
    uint8_t status;
    BteBdAddr address;
} BteHciLinkKeyReqReply;

typedef void (*BteHciLinkKeyReqReplyCb)(BteHci *hci,
                                        const BteHciLinkKeyReqReply *reply,
                                        void *userdata);
void bte_hci_link_key_req_reply(BteHci *hci, const BteBdAddr *address,
                                const BteLinkKey *key,
                                BteHciLinkKeyReqReplyCb callback);
void bte_hci_link_key_req_neg_reply(BteHci *hci, const BteBdAddr *address,
                                    BteHciLinkKeyReqReplyCb callback);

/* Return true if this client will handle the event */
typedef bool (*BteHciPinCodeRequestCb)(BteHci *hci,
                                       const BteBdAddr *address,
                                       void *userdata);
void bte_hci_on_pin_code_request(BteHci *hci, BteHciPinCodeRequestCb callback);

typedef struct {
    uint8_t status;
    BteBdAddr address;
} BteHciPinCodeReqReply;

typedef void (*BteHciPinCodeReqReplyCb)(BteHci *hci,
                                        const BteHciPinCodeReqReply *reply,
                                        void *userdata);
void bte_hci_pin_code_req_reply(BteHci *hci, const BteBdAddr *address,
                                const uint8_t *pin, uint8_t len,
                                BteHciPinCodeReqReplyCb callback);
void bte_hci_pin_code_req_neg_reply(BteHci *hci, const BteBdAddr *address,
                                    BteHciPinCodeReqReplyCb callback);

/* Controller & baseband commands */

void bte_hci_set_event_mask(BteHci *hci, BteHciEventMask mask,
                            BteHciDoneCb callback);
void bte_hci_reset(BteHci *hci, BteHciDoneCb callback);

#define BTE_HCI_EVENT_FILTER_TYPE_CLEAR            (uint8_t)0
#define BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT   (uint8_t)1
#define BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP (uint8_t)2

#define BTE_HCI_COND_TYPE_INQUIRY_ALL     (uint8_t)0
#define BTE_HCI_COND_TYPE_INQUIRY_COD     (uint8_t)1
#define BTE_HCI_COND_TYPE_INQUIRY_ADDRESS (uint8_t)2

#define BTE_HCI_COND_TYPE_CONN_SETUP_ALL     (uint8_t)0
#define BTE_HCI_COND_TYPE_CONN_SETUP_COD     (uint8_t)1
#define BTE_HCI_COND_TYPE_CONN_SETUP_ADDRESS (uint8_t)2

#define BTE_HCI_COND_VALUE_CONN_SETUP_AUTO_OFF   (uint8_t)1
#define BTE_HCI_COND_VALUE_CONN_SETUP_SWITCH_OFF (uint8_t)2
#define BTE_HCI_COND_VALUE_CONN_SETUP_SWITCH_ON  (uint8_t)3

void bte_hci_set_event_filter(BteHci *hci, uint8_t filter_type,
                              uint8_t cond_type, const void *filter_data,
                              BteHciDoneCb callback);

#define BTE_HCI_PIN_TYPE_VARIABLE (uint8_t)0
#define BTE_HCI_PIN_TYPE_FIXED    (uint8_t)1

typedef struct {
    uint8_t status;
    uint8_t pin_type;
} BteHciReadPinTypeReply;

typedef void (*BteHciReadPinTypeCb)(BteHci *hci,
                                    const BteHciReadPinTypeReply *reply,
                                    void *userdata);
void bte_hci_read_pin_type(BteHci *hci, BteHciReadPinTypeCb callback);
void bte_hci_write_pin_type(BteHci *hci, uint8_t pin_type,
                            BteHciDoneCb callback);

typedef struct {
    BteBdAddr address;
    BteLinkKey key;
} BTE_PACKED BteHciStoredLinkKey;

typedef struct {
    uint8_t status;
    uint16_t max_keys;
    uint16_t num_keys;
    const BteHciStoredLinkKey *stored_keys;
} BteHciReadStoredLinkKeyReply;

typedef void (*BteHciReadStoredLinkKeyCb)(
    BteHci *hci, const BteHciReadStoredLinkKeyReply *reply, void *userdata);
void bte_hci_read_stored_link_key(BteHci *hci, const BteBdAddr *address,
                                  BteHciReadStoredLinkKeyCb callback);

typedef struct {
    uint8_t status;
    uint8_t num_keys;
} BteHciWriteStoredLinkKeyReply;

typedef void (*BteHciWriteStoredLinkKeyCb)(
    BteHci *hci, const BteHciWriteStoredLinkKeyReply *reply, void *userdata);
void bte_hci_write_stored_link_key(BteHci *hci, int num_keys,
                                   const BteHciStoredLinkKey *keys,
                                   BteHciWriteStoredLinkKeyCb callback);

typedef struct {
    uint8_t status;
    uint16_t num_keys;
} BteHciDeleteStoredLinkKeyReply;

typedef void (*BteHciDeleteStoredLinkKeyCb)(
    BteHci *hci, const BteHciDeleteStoredLinkKeyReply *reply, void *userdata);
void bte_hci_delete_stored_link_key(BteHci *hci, const BteBdAddr *address,
                                    BteHciDeleteStoredLinkKeyCb callback);

void bte_hci_write_local_name(BteHci *hci, const char *name,
                              BteHciDoneCb callback);

typedef struct {
    uint8_t status;
    char name[248 + 1];
} BteHciReadLocalNameReply;

typedef void (*BteHciReadLocalNameCb)(BteHci *hci,
                                      const BteHciReadLocalNameReply *reply,
                                      void *userdata);
void bte_hci_read_local_name(BteHci *hci, BteHciReadLocalNameCb callback);

typedef struct {
    uint8_t status;
    uint16_t page_timeout;
} BteHciReadPageTimeoutReply;

typedef void (*BteHciReadPageTimeoutCb)(
    BteHci *hci, const BteHciReadPageTimeoutReply *reply, void *userdata);
void bte_hci_read_page_timeout(BteHci *hci, BteHciReadPageTimeoutCb callback);
void bte_hci_write_page_timeout(BteHci *hci, uint16_t page_timeout,
                                BteHciDoneCb callback);

void bte_hci_write_class_of_device(BteHci *hci, const BteClassOfDevice *cod,
                                   BteHciDoneCb callback);

/* Informational parameters */

typedef struct {
    uint8_t status;
    uint8_t hci_version;
    uint16_t hci_revision;
    uint8_t lmp_version;
    uint16_t manufacturer;
    uint16_t lmp_subversion;
} BteHciReadLocalVersionReply;

typedef void (*BteHciReadLocalVersionCb)(
    BteHci *hci, const BteHciReadLocalVersionReply *reply, void *userdata);
void bte_hci_read_local_version(BteHci *hci,
                                BteHciReadLocalVersionCb callback);

typedef struct {
    uint8_t status;
    BteHciSupportedFeatures features;
} BteHciReadLocalFeaturesReply;

typedef void (*BteHciReadLocalFeaturesCb)(
    BteHci *hci, const BteHciReadLocalFeaturesReply *reply, void *userdata);
void bte_hci_read_local_features(BteHci *hci,
                                 BteHciReadLocalFeaturesCb callback);

typedef struct {
    uint8_t status;
    uint8_t sco_mtu;
    uint16_t acl_mtu;
    uint16_t sco_max_packets;
    uint16_t acl_max_packets;
} BteHciReadBufferSizeReply;

typedef void (*BteHciReadBufferSizeCb)(BteHci *hci,
                                       const BteHciReadBufferSizeReply *reply,
                                       void *userdata);
void bte_hci_read_buffer_size(BteHci *hci, BteHciReadBufferSizeCb callback);

typedef struct {
    uint8_t status;
    BteBdAddr address;
} BteHciReadBdAddrReply;

typedef void (*BteHciReadBdAddrCb)(BteHci *hci,
                                   const BteHciReadBdAddrReply *reply,
                                   void *userdata);
void bte_hci_read_bd_addr(BteHci *hci, BteHciReadBdAddrCb callback);

typedef void (*BteHciVendorCommandCb)(BteHci *hci,
                                      BteBuffer *reply_data,
                                      void *userdata);
void bte_hci_vendor_command(BteHci *hci, uint16_t ocf,
                            const void *data, uint8_t len,
                            BteHciVendorCommandCb callback);

#ifdef __cplusplus
}
#endif

#endif /* BTE_HCI_H */
