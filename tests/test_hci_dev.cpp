#include "bt-embedded/internals.h"
#include "mock_backend.h"

#include <gtest/gtest.h>

TEST(HciDev, testSendQueuedData)
{
    MockBackend backend;

    BteHciDev *dev = &_bte_hci_dev;
    memset(dev, 0, sizeof(*dev));
    _bte_hci_dev_init();

    uint16_t acl_mtu = 10;
    uint16_t acl_max_packets = 3;
    _bte_hci_dev_set_buffer_size(acl_mtu, acl_max_packets,
                                 /* We don't care about SCO here */
                                 42, 42);
    EXPECT_EQ(dev->acl_available_packets, 3);

    /* Try with an empty queue */
    int rc = _bte_hci_send_queued_data();
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(dev->acl_available_packets, 3);
    EXPECT_TRUE(backend.sentData().empty());

    /* Now try sending some packets */
    char text[] = "Some text that must be fragmented and then reassembled";
    Buffer data { text };
    BteBuffer *packets = data.toBuffer(acl_mtu);

    dev->outgoing_acl_packets = packets;
    rc = _bte_hci_send_queued_data();
    EXPECT_EQ(rc, 3);
    EXPECT_EQ(dev->acl_available_packets, 0);
    EXPECT_EQ(backend.sentData().size(), 3);

    /* No packets available, should do nothing */
    rc = _bte_hci_send_queued_data();
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(dev->acl_available_packets, 0);
    EXPECT_EQ(backend.sentData().size(), 3);

    /* Pretend that the controller has reported a packet complete event */
    _bte_hci_dev_on_completed_packets(1);
    EXPECT_EQ(dev->acl_available_packets, 0);
    EXPECT_EQ(backend.sentData().size(), 4);

    /* And finally is totally free */
    _bte_hci_dev_on_completed_packets(3);
    EXPECT_EQ(dev->acl_available_packets, 1);
    EXPECT_EQ(backend.sentData().size(), 6);
    std::vector<Buffer> expectedData = {
        {data.begin(), data.begin() + 10},
        {data.begin() + 10, data.begin() + 20},
        {data.begin() + 20, data.begin() + 30},
        {data.begin() + 30, data.begin() + 40},
        {data.begin() + 40, data.begin() + 50},
        {"bled"},
    };
    ASSERT_EQ(backend.sentData(), expectedData);
}
