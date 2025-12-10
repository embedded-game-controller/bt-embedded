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

static const uint8_t wii_patch_0[184] = {
    0x70, 0x99, 0x08, 0x00, 0x88, 0x43, 0xd1, 0x07, 0x09, 0x0c, 0x08, 0x43,
    0xa0, 0x62, 0x19, 0x23, 0xdb, 0x01, 0x33, 0x80, 0x7c, 0xf7, 0x88, 0xf8,
    0x28, 0x76, 0x80, 0xf7, 0x17, 0xff, 0x43, 0x78, 0xeb, 0x70, 0x19, 0x23,
    0xdb, 0x01, 0x33, 0x87, 0x7c, 0xf7, 0xbc, 0xfb, 0x0b, 0x60, 0xa3, 0x7b,
    0x01, 0x49, 0x0b, 0x60, 0x90, 0xf7, 0x96, 0xfb, 0xd8, 0x1d, 0x08, 0x00,
    0x00, 0xf0, 0x04, 0xf8, 0x00, 0x23, 0x79, 0xf7, 0xe3, 0xfa, 0x00, 0x00,
    0x00, 0xb5, 0x00, 0x23, 0x11, 0x49, 0x0b, 0x60, 0x1d, 0x21, 0xc9, 0x03,
    0x0b, 0x60, 0x7d, 0x20, 0x80, 0x01, 0x01, 0x38, 0xfd, 0xd1, 0x0e, 0x4b,
    0x0e, 0x4a, 0x13, 0x60, 0x47, 0x20, 0x00, 0x21, 0x96, 0xf7, 0x96, 0xff,
    0x46, 0x20, 0x00, 0x21, 0x96, 0xf7, 0x92, 0xff, 0x0a, 0x4a, 0x13, 0x68,
    0x0a, 0x48, 0x03, 0x40, 0x13, 0x60, 0x0a, 0x4a, 0x13, 0x68, 0x0a, 0x48,
    0x03, 0x40, 0x13, 0x60, 0x09, 0x4a, 0x13, 0x68, 0x09, 0x48, 0x03, 0x40,
    0x13, 0x60, 0x00, 0xbd, 0x24, 0x80, 0x0e, 0x00, 0x81, 0x03, 0x0f, 0xfe,
    0x5c, 0x00, 0x0f, 0x00, 0x60, 0xfc, 0x0e, 0x00, 0xfe, 0xff, 0x00, 0x00,
    0xfc, 0xfc, 0x0e, 0x00, 0xff, 0x9f, 0x00, 0x00, 0x30, 0xfc, 0x0e, 0x00,
    0x7f, 0xff, 0x00, 0x00
};
static const uint8_t wii_patch_1[92] = {
    0x07, 0x20, 0xbc, 0x65, 0x01, 0x00, 0x84, 0x42, 0x09, 0xd2, 0x84, 0x42,
    0x09, 0xd1, 0x21, 0x84, 0x5a, 0x00, 0x00, 0x83, 0xf0, 0x74, 0xff, 0x09,
    0x0c, 0x08, 0x43, 0x22, 0x00, 0x61, 0x00, 0x00, 0x83, 0xf0, 0x40, 0xfc,
    0x00, 0x00, 0x00, 0x00, 0x23, 0xcc, 0x9f, 0x01, 0x00, 0x6f, 0xf0, 0xe4,
    0xfc, 0x03, 0x28, 0x7d, 0xd1, 0x24, 0x3c, 0x62, 0x01, 0x00, 0x28, 0x20,
    0x00, 0xe0, 0x60, 0x8d, 0x23, 0x68, 0x25, 0x04, 0x12, 0x01, 0x00, 0x20,
    0x1c, 0x20, 0x1c, 0x24, 0xe0, 0xb0, 0x21, 0x26, 0x74, 0x2f, 0x00, 0x00,
    0x86, 0xf0, 0x18, 0xfd, 0x21, 0x4f, 0x3b, 0x60
};

static void initialization_done(BteClient *client)
{
    BTE_DEBUG("%s\n", __func__);
    bte_client_unref(client);
    _bte_hci_dev_set_status(BTE_HCI_INIT_STATUS_INITIALIZED);
}

static void on_reset_patched_done(BteHci *hci, const BteHciReply *reply, void *)
{
    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    /* TODO libogc here would call:
     *   hci_read_buffer_size()
     *   hci_write_cod({0x04, 0x02,0x40})
     *   hci_write_local_name((u8_t*)"",1)
     *   hci_write_pin_type(0x00)
     *   hci_host_buffer_size()
     *   hci_read_local_version()
     *   hci_read_bd_addr()
     *   hci_read_local_features()
     * Then BTE_InitSub(), which calls
     *   hci_write_inquiry_mode(0x01)
     *   hci_write_page_scan_type(0x01)
     *   hci_write_inquiry_scan_type(0x01)
     *   hci_write_cod({0x00, 0x04,0x48})
     *   hci_write_page_timeout(0x8000)
     *   hci_write_local_name((u8_t*)"Wii",4)
     *   hci_write_scan_enable(0x02)
     * Then BTE_SetEvtFilter(0x01,0x00,NULL,...), which calls
     *   hci_set_event_filter(0x01,0x00,NULL)
     */
    initialization_done(bte_hci_get_client(hci));
}

static void on_patch_end_done(BteHci *hci, BteBuffer *reply, void *userdata)
{
    BTE_DEBUG("%s\n", __func__);

    bte_hci_reset(hci, on_reset_patched_done, userdata);
}

static void on_patch_cont_done(BteHci *hci, BteBuffer *reply, void *userdata)
{
    BTE_DEBUG("%s\n", __func__);

    bte_hci_vendor_command(hci, HCI_VENDOR_PATCH_END_OCF,
                           wii_patch_1, sizeof(wii_patch_1),
                           on_patch_end_done, userdata);
}

static void on_patch_start_done(BteHci *hci, BteBuffer *reply, void *userdata)
{
    BTE_DEBUG("%s\n", __func__);

    bte_hci_vendor_command(hci, HCI_VENDOR_PATCH_CONT_OCF,
                           wii_patch_0, sizeof(wii_patch_0),
                           on_patch_cont_done, userdata);
}

static void on_read_local_features(BteHci *hci,
                                   const BteHciReadLocalFeaturesReply *reply,
                                   void *userdata)
{
    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    /* TODO libogc here would call:
     *   hci_write_inquiry_mode(0x01)
     *   hci_write_page_scan_type(0x01)
     *   hci_write_inquiry_scan_type(0x01)
     *   hci_write_cod({0x04, 0x02,0x40})
     *   hci_write_page_timeout(0x2000)
     * And this would be the end of BTE_InitCore. Then it calls
     * BTE_ReadStoredLinkKey():
     *   hci_read_stored_link_key()
     * Then BTE_ApplyPatch(), which calls:
     *   hci_vendor_specific_command(HCI_VENDOR_PATCH_START_OCF,
     *                               HCI_VENDOR_OGF,&kick,1)
     */
    uint8_t kick = 0;
    bte_hci_vendor_command(hci, HCI_VENDOR_PATCH_START_OCF,
                           &kick, sizeof(kick),
                           on_patch_start_done, userdata);
}

static void on_bd_addr_done(BteHci *hci, const BteHciReadBdAddrReply *reply,
                            void *userdata)
{
    BteHciDev *dev = userdata;

    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    dev->address = reply->address;
    bte_hci_read_local_features(hci, on_read_local_features, userdata);
}

static void on_read_local_version_done(
    BteHci *hci, const BteHciReadLocalVersionReply *reply, void *userdata)
{
    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    bte_hci_read_bd_addr(hci, on_bd_addr_done, userdata);
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
    /* TODO libogc here would call:
     * - write_cod
     * - write_local_name
     * - write_pin_type
     * - read_host_buffer_size
     * - read_local_version
     */
    bte_hci_read_local_version(hci, on_read_local_version_done, userdata);
}

static void on_reset_done(BteHci *hci, const BteHciReply *reply,
                          void *userdata)
{
    BTE_DEBUG("%s\n", __func__);
    STOP_ON_FAILURE(hci, reply);

    bte_hci_read_buffer_size(hci, on_buffer_size_done, userdata);
}

static int wii_init(BteHciDev *dev)
{
    BteClient *client = bte_client_new();
    bte_client_set_userdata(client, dev);
    BteHci *hci = bte_hci_get(client);

    bte_hci_reset(hci, on_reset_done, dev);
    return 0;
}

const BteDriver _bte_driver = {
    .init = wii_init,
};
