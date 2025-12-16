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

/* Functions for the tests */
void dummy_driver_set_acl_limits(uint16_t acl_mtu, uint16_t acl_max_packets)
{
    _bte_hci_dev.acl_mtu = acl_mtu;
    _bte_hci_dev.acl_max_packets = acl_max_packets;
}
