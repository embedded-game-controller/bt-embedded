#ifndef BTE_HCI_H
#define BTE_HCI_H

#include "hci_proto.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t BteHciEventMask;
typedef uint16_t BteHciConnHandle;
typedef uint16_t BteHciLinkPolicySettings;

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
void bte_hci_inquiry(BteHci *hci, BteLap lap, uint8_t len, uint8_t max_resp,
                     BteHciDoneCb status_cb,
                     BteHciInquiryCb callback);
void bte_hci_inquiry_cancel(BteHci *hci, BteHciDoneCb callback);
void bte_hci_periodic_inquiry(BteHci *hci,
                              uint16_t min_period, uint16_t max_period,
                              BteLap lap, uint8_t len, uint8_t max_resp,
                              BteHciDoneCb status_cb,
                              BteHciInquiryCb callback);
void bte_hci_exit_periodic_inquiry(BteHci *hci, BteHciDoneCb callback);

#define BTE_HCI_CLOCK_OFFSET_INVALID (uint16_t)0xffff

typedef struct {
    uint8_t status;
    uint8_t link_type;
    BteHciConnHandle conn_handle;
    BteBdAddr address;
    uint8_t encryption_mode;
} BteHciCreateConnectionReply;

typedef void (*BteHciCreateConnectionCb)(
    BteHci *hci, const BteHciCreateConnectionReply *reply, void *userdata);
void bte_hci_create_connection(BteHci *hci,
                               const BteBdAddr *address,
                               BtePacketType packet_type,
                               uint8_t page_scan_rep_mode,
                               uint16_t clock_offset,
                               bool allow_role_switch,
                               BteHciDoneCb status_cb,
                               BteHciCreateConnectionCb callback);
void bte_hci_disconnect(BteHci *hci, BteHciConnHandle handle, uint8_t reason,
                        BteHciDoneCb callback);
void bte_hci_create_connection_cancel(BteHci *hci, const BteBdAddr *address,
                                      BteHciDoneCb callback);


#define BTE_HCI_ROLE_MASTER (uint8_t)0
#define BTE_HCI_ROLE_SLAVE  (uint8_t)1

typedef BteHciCreateConnectionReply BteHciAcceptConnectionReply;
typedef void (*BteHciAcceptConnectionCb)(
    BteHci *hci, const BteHciAcceptConnectionReply *reply, void *userdata);
void bte_hci_accept_connection(BteHci *hci,
                               const BteBdAddr *address, uint8_t role,
                               BteHciDoneCb status_cb,
                               BteHciAcceptConnectionCb callback);

typedef BteHciCreateConnectionReply BteHciRejectConnectionReply;
typedef void (*BteHciRejectConnectionCb)(
    BteHci *hci, const BteHciRejectConnectionReply *reply, void *userdata);
void bte_hci_reject_connection(BteHci *hci,
                               const BteBdAddr *address, uint8_t reason,
                               BteHciDoneCb status_cb,
                               BteHciRejectConnectionCb callback);

/* Return true if this client will handle the event */
typedef bool (*BteHciConnectionRequestCb)(BteHci *hci,
                                          const BteBdAddr *address,
                                          const BteClassOfDevice *cod,
                                          uint8_t link_type,
                                          void *userdata);
void bte_hci_on_connection_request(BteHci *hci, BteHciConnectionRequestCb callback);

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

typedef struct {
    uint8_t status;
    BteHciConnHandle conn_handle;
} BteHciAuthRequestedReply;

typedef void (*BteHciAuthRequestedCb)(
    BteHci *hci, const BteHciAuthRequestedReply *reply, void *userdata);
void bte_hci_auth_requested(BteHci *hci,
                            BteHciConnHandle conn_handle,
                            BteHciDoneCb status_cb,
                            BteHciAuthRequestedCb callback);

typedef struct {
    uint8_t status;
    BteBdAddr address;
    char name[248 + 1];
} BteHciReadRemoteNameReply;

typedef void (*BteHciReadRemoteNameCb)(
    BteHci *hci, const BteHciReadRemoteNameReply *reply, void *userdata);
void bte_hci_read_remote_name(BteHci *hci,
                              const BteBdAddr *address,
                              uint8_t page_scan_rep_mode,
                              uint16_t clock_offset,
                              BteHciDoneCb status_cb,
                              BteHciReadRemoteNameCb callback);

typedef struct {
    uint8_t status;
    BteHciConnHandle conn_handle;
    uint16_t clock_offset;
} BteHciReadClockOffsetReply;

typedef void (*BteHciReadClockOffsetCb)(
    BteHci *hci, const BteHciReadClockOffsetReply *reply, void *userdata);
void bte_hci_read_clock_offset(BteHci *hci,
                               BteHciConnHandle conn_handle,
                               BteHciDoneCb status_cb,
                               BteHciReadClockOffsetCb callback);

/* Link policy commands */

#define BTE_HCI_MODE_ACTIVE (uint8_t)0
#define BTE_HCI_MODE_HOLD   (uint8_t)1
#define BTE_HCI_MODE_SNIFF  (uint8_t)2
#define BTE_HCI_MODE_PARK   (uint8_t)3

/* Use bte_hci_on_mode_change() to be notified of the change */
void bte_hci_set_sniff_mode(BteHci *hci, BteHciConnHandle conn_handle,
                            uint16_t min_interval, uint16_t max_interval,
                            uint16_t attempt_slots, uint16_t timeout,
                            BteHciDoneCb status_cb);

typedef struct {
    uint8_t status;
    BteHciConnHandle conn_handle;
    uint8_t current_mode;
    uint16_t interval;
} BteHciModeChangeReply;
typedef bool (*BteHciModeChangeCb)(BteHci *hci,
                                   const BteHciModeChangeReply *reply,
                                   void *userdata);
void bte_hci_on_mode_change(BteHci *hci, BteHciConnHandle conn_handle,
                            BteHciModeChangeCb callback);

#define BTE_HCI_LINK_POLICY_SETTINGS_DISABLE (uint8_t)0
#define BTE_HCI_LINK_POLICY_SETTINGS_ROLE_SW (uint8_t)1
#define BTE_HCI_LINK_POLICY_SETTINGS_HOLD    (uint8_t)2
#define BTE_HCI_LINK_POLICY_SETTINGS_SNIFF   (uint8_t)4
#define BTE_HCI_LINK_POLICY_SETTINGS_PARK    (uint8_t)8

typedef struct {
    uint8_t status;
    BteHciConnHandle conn_handle;
    BteHciLinkPolicySettings settings;
} BteHciReadLinkPolicySettingsReply;

typedef void (*BteHciReadLinkPolicySettingsCb)(
    BteHci *hci, const BteHciReadLinkPolicySettingsReply *reply,
    void *userdata);
void bte_hci_read_link_policy_settings(
    BteHci *hci, BteHciConnHandle conn_handle,
    BteHciReadLinkPolicySettingsCb callback);
void bte_hci_write_link_policy_settings(BteHci *hci,
                                        BteHciConnHandle conn_handle,
                                        BteHciLinkPolicySettings settings,
                                        BteHciDoneCb callback);

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

#define BTE_HCI_SCAN_ENABLE_OFF      (uint8_t)0
#define BTE_HCI_SCAN_ENABLE_INQUIRY  (uint8_t)1
#define BTE_HCI_SCAN_ENABLE_PAGE     (uint8_t)2
#define BTE_HCI_SCAN_ENABLE_INQ_PAGE (uint8_t)3

typedef struct {
    uint8_t status;
    uint8_t scan_enable;
} BteHciReadScanEnableReply;

typedef void (*BteHciReadScanEnableCb)(BteHci *hci,
                                       const BteHciReadScanEnableReply *reply,
                                       void *userdata);
void bte_hci_read_scan_enable(BteHci *hci, BteHciReadScanEnableCb callback);
void bte_hci_write_scan_enable(BteHci *hci, uint8_t scan_enable,
                               BteHciDoneCb callback);

#define BTE_HCI_AUTH_ENABLE_OFF (uint8_t)0
#define BTE_HCI_AUTH_ENABLE_ON  (uint8_t)1

typedef struct {
    uint8_t status;
    uint8_t auth_enable;
} BteHciReadAuthEnableReply;

typedef void (*BteHciReadAuthEnableCb)(BteHci *hci,
                                       const BteHciReadAuthEnableReply *reply,
                                       void *userdata);
void bte_hci_read_auth_enable(BteHci *hci, BteHciReadAuthEnableCb callback);
void bte_hci_write_auth_enable(BteHci *hci, uint8_t auth_enable,
                               BteHciDoneCb callback);

typedef struct {
    uint8_t status;
    BteClassOfDevice cod;
} BteHciReadClassOfDeviceReply;

typedef void (*BteHciReadClassOfDeviceCb)(
    BteHci *hci, const BteHciReadClassOfDeviceReply *reply, void *userdata);
void bte_hci_read_class_of_device(BteHci *hci,
                                  BteHciReadClassOfDeviceCb callback);
void bte_hci_write_class_of_device(BteHci *hci, const BteClassOfDevice *cod,
                                   BteHciDoneCb callback);

typedef struct {
    uint8_t status;
    BteHciConnHandle conn_handle;
    uint16_t flush_timeout;
} BteHciReadAutoFlushTimeoutReply;

typedef void (*BteHciReadAutoFlushTimeoutCb)(
    BteHci *hci, const BteHciReadAutoFlushTimeoutReply *reply, void *userdata);
void bte_hci_read_auto_flush_timeout(BteHci *hci,
                                     BteHciConnHandle conn_handle,
                                     BteHciReadAutoFlushTimeoutCb callback);
void bte_hci_write_auto_flush_timeout(BteHci *hci,
                                      BteHciConnHandle conn_handle,
                                      uint16_t timeout,
                                      BteHciDoneCb callback);

#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_OFF      (uint8_t)0
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_ACL      (uint8_t)1
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_SYNC     (uint8_t)2
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_ACL_SYNC (uint8_t)3

void bte_hci_set_ctrl_to_host_flow_control(BteHci *hci, uint8_t enable,
                                           BteHciDoneCb callback);

void bte_hci_set_host_buffer_size(BteHci *hci,
                                  uint16_t acl_packet_len,
                                  uint16_t acl_packets,
                                  uint8_t sync_packet_len,
                                  uint16_t sync_packets,
                                  BteHciDoneCb callback);

typedef struct {
    uint8_t status;
    uint8_t num_laps;
    const BteLap *laps;
} BteHciReadCurrentIacLapReply;

typedef void (*BteHciReadCurrentIacLapCb)(
    BteHci *hci, const BteHciReadCurrentIacLapReply *reply, void *userdata);
void bte_hci_read_current_iac_lap(BteHci *hci,
                                  BteHciReadCurrentIacLapCb callback);
void bte_hci_write_current_iac_lap(BteHci *hci,
                                   uint8_t num_laps, const BteLap *laps,
                                   BteHciDoneCb callback);

void bte_hci_host_num_comp_packets(BteHci *hci,
                                   BteHciConnHandle conn_handle,
                                   uint16_t num_packets);

typedef struct {
    uint8_t status;
    BteHciConnHandle conn_handle;
    uint16_t sv_timeout;
} BteHciReadLinkSvTimeoutReply;

typedef void (*BteHciReadLinkSvTimeoutCb)(
    BteHci *hci, const BteHciReadLinkSvTimeoutReply *reply, void *userdata);
void bte_hci_read_link_sv_timeout(BteHci *hci,
                                  BteHciConnHandle conn_handle,
                                  BteHciReadLinkSvTimeoutCb callback);
void bte_hci_write_link_sv_timeout(BteHci *hci,
                                   BteHciConnHandle conn_handle,
                                   uint16_t timeout,
                                   BteHciDoneCb callback);

#define BTE_HCI_INQUIRY_SCAN_TYPE_STANDARD   (uint8_t)0
#define BTE_HCI_INQUIRY_SCAN_TYPE_INTERLACED (uint8_t)1

typedef struct {
    uint8_t status;
    uint8_t inquiry_scan_type;
} BteHciReadInquiryScanTypeReply;

typedef void (*BteHciReadInquiryScanTypeCb)(
    BteHci *hci, const BteHciReadInquiryScanTypeReply *reply, void *userdata);
void bte_hci_read_inquiry_scan_type(BteHci *hci,
                                    BteHciReadInquiryScanTypeCb callback);
void bte_hci_write_inquiry_scan_type(BteHci *hci, uint8_t inquiry_scan_type,
                                     BteHciDoneCb callback);

#define BTE_HCI_INQUIRY_MODE_STANDARD (uint8_t)0
#define BTE_HCI_INQUIRY_MODE_RSSI     (uint8_t)1

typedef struct {
    uint8_t status;
    uint8_t inquiry_mode;
} BteHciReadInquiryModeReply;

typedef void (*BteHciReadInquiryModeCb)(
    BteHci *hci, const BteHciReadInquiryModeReply *reply, void *userdata);
void bte_hci_read_inquiry_mode(BteHci *hci, BteHciReadInquiryModeCb callback);
void bte_hci_write_inquiry_mode(BteHci *hci, uint8_t inquiry_mode,
                                BteHciDoneCb callback);

#define BTE_HCI_PAGE_SCAN_TYPE_STANDARD   (uint8_t)0
#define BTE_HCI_PAGE_SCAN_TYPE_INTERLACED (uint8_t)1

typedef struct {
    uint8_t status;
    uint8_t page_scan_type;
} BteHciReadPageScanTypeReply;

typedef void (*BteHciReadPageScanTypeCb)(
    BteHci *hci, const BteHciReadPageScanTypeReply *reply, void *userdata);
void bte_hci_read_page_scan_type(BteHci *hci, BteHciReadPageScanTypeCb callback);
void bte_hci_write_page_scan_type(BteHci *hci, uint8_t page_scan_type,
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
