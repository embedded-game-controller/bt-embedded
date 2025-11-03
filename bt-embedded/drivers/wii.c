#include "client.h"
#include "driver.h"
#include "internals.h"
#include "logging.h"
#include "utils.h"

#include <assert.h>

#define STOP_ON_FAILURE(hci, reply)                                            \
    if (reply->status != HCI_SUCCESS) {                                        \
        _bte_hci_dev_set_status(BTE_HCI_INIT_STATUS_FAILED);                   \
        bte_client_unref(bte_hci_get_client(hci));                             \
        return;                                                                \
    }

static void initialization_done(BteClient *client)
{
    BTE_DEBUG("%s\n", __func__);
    bte_client_unref(client);
    _bte_hci_dev_set_status(BTE_HCI_INIT_STATUS_INITIALIZED);
}

static void on_reset_done(BteHci *hci, const BteHciReply *reply, void *)
{
    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    initialization_done(bte_hci_get_client(hci));
}

static void on_read_local_features(BteHci *hci,
                                   const BteHciReadLocalFeaturesReply *reply,
                                   void *)
{
    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    bte_hci_reset(hci, on_reset_done);
}

static void on_bd_addr_done(BteHci *hci, const BteHciReadBdAddrReply *reply,
                            void *userdata)
{
    BteHciDev *dev = userdata;

    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    dev->address = reply->address;
    bte_hci_read_local_features(hci, on_read_local_features);
}

static void on_buffer_size_done(BteHci *hci,
                                const BteHciReadBufferSizeReply *reply,
                                void *userdata)
{
    BteHciDev *dev = userdata;

    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    dev->acl_mtu = reply->acl_mtu;
    dev->sco_mtu = reply->sco_mtu;
    dev->acl_max_packets = reply->acl_max_packets;
    dev->sco_max_packets = reply->sco_max_packets;
    dev->info_flags |= BTE_HCI_INFO_GOT_BUFFER_SIZE;
    bte_hci_read_bd_addr(hci, on_bd_addr_done);
}

static int wii_init(BteHciDev *dev)
{
    BteClient *client = bte_client_new();
    bte_client_set_userdata(client, dev);
    BteHci *hci = bte_hci_get(client);
    BTE_DEBUG("%s drier HCI = %p\n", __func__, hci);

    bte_hci_read_buffer_size(hci, on_buffer_size_done);
    return 0;
}

const BteDriver _bte_driver = {
    .init = wii_init,
};
