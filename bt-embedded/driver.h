#ifndef BTE_DRIVER_H
#define BTE_DRIVER_H

#include "internals.h"

typedef struct bte_driver_t BteDriver;

/* Interface for BT drivers. */
struct bte_driver_t {
    /* The driver should setup the BT device so that it's ready for use,
     * then call _bte_hci_dev_set_status(BTE_HCI_INIT_STATUS_INITIALIZED). */
    int (*init)(BteHciDev *dev);
};

/* At the moment, we support one fixed driver per platform. But the API is
 * designed to support more in the future, should the need arise (for instance,
 * if the BT chip has changed during the lifetime of the platform). */
extern const BteDriver _bte_driver;

#endif
