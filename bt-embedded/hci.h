#ifndef BTE_HCI_H
#define BTE_HCI_H

#include "hci_proto.h"
#include "types.h"

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
void bte_hci_reset(BteHci *hci, BteHciDoneCb callback);

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

void bte_hci_write_class_of_device(BteHci *hci, const BteClassOfDevice *cod,
                                   BteHciDoneCb callback);

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

#endif /* BTE_HCI_H */
