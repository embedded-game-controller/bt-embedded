#include "bt-embedded/driver.h"
#include "bt-embedded/internals.h"

static int dummy_init(BteHciDev *dev)
{
    _bte_hci_dev_set_status(BTE_HCI_INIT_STATUS_INITIALIZED);
    return 0;
}

const BteDriver _bte_driver = {
    .init = dummy_init,
};
