#include "l2cap_fixtures.h"

#include <gtest/gtest.h>
#include <memory>

class TestL2capNewConfigured: public TestL2capFixture
{
protected:
    void SetUp() override {
        TestL2capFixture::SetUp();
        m_connHandle = 0x0100;
        m_remoteCid = 0x0040;
    }
};

TEST_F(TestL2capNewConfigured, testDefaults) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capNewConfiguredReply> replies;
    L2cap l2cap;
    auto onConnected = [&](std::optional<Bte::L2cap> l2capOpt,
                           const BteL2capNewConfiguredReply &reply) {
        ASSERT_TRUE(l2capOpt.has_value());
        replies.push_back(reply);
        l2cap = l2capOpt.value();
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newConfigured(m_client, address, psm, {}, 0, {}, onConnected);

    /* Default values */
    BtePacketType packetType = BTE_PACKET_TYPE_DM1 | BTE_PACKET_TYPE_DH1;
    uint16_t clockOffset = 0;
    uint8_t pageScanRepMode = 1;
    bool roleSwitch = true;
    std::vector<Buffer> expectedCommands {
        makeHciCreateConnection(address, packetType, pageScanRepMode,
                                clockOffset, roleSwitch),
    };
    ASSERT_EQ(m_backend.sentCommands(), expectedCommands);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address, m_connHandle);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    sendConnectResponse(reqId);
    bte_handle_events();

    /* Read the configure request */
    reqId = m_cmdId++;
    expectedData = makeConfigRequest({}, reqId, m_remoteCid);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send an empty configure reply */
    sendConfigResponse(Buffer(), reqId);
    bte_handle_events();

    /* And send the peer's configuration request */
    sendConfigRequest({}, 51);
    bte_handle_events();

    /* Now the connection should be open */
    std::vector<BteL2capNewConfiguredReply> expectedReplies = {
        {BTE_L2CAP_CONN_RESP_RES_OK},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capNewConfigured, testConnectHciError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capNewConfiguredReply> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2capOpt,
                           const BteL2capNewConfiguredReply &reply) {
        ASSERT_FALSE(l2capOpt.has_value());
        replies.push_back(reply);
    };

    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    BtePacketType packetType = BTE_PACKET_TYPE_DM3;
    uint16_t clockOffset = 0x1234;
    uint8_t pageScanRepMode = 2;
    bool roleSwitch = false;
    BteHciConnectParams params = {
        packetType, clockOffset, pageScanRepMode, roleSwitch
    };
    L::newConfigured(m_client, address, psm, params, 0, {}, onConnected);

    std::vector<Buffer> expectedCommands {
        makeHciCreateConnection(address, packetType, pageScanRepMode,
                                clockOffset, roleSwitch),
    };
    ASSERT_EQ(m_backend.sentCommands(), expectedCommands);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address, 0, BTE_HCI_CONN_TIMEOUT);
    bte_handle_events();

    ASSERT_TRUE(m_backend.sentData().empty());

    std::vector<BteL2capNewConfiguredReply> expectedReplies = {
        {BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capNewConfigured, testConnectError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capNewConfiguredReply> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2capOpt,
                           const BteL2capNewConfiguredReply &reply) {
        ASSERT_FALSE(l2capOpt.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newConfigured(m_client, address, psm, {}, 0, {}, onConnected);

    /* This is the HCI connect request */
    ASSERT_EQ(m_backend.sentCommands().size(), 1);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address, m_connHandle);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    sendConnectResponse(reqId, BTE_L2CAP_CONN_RESP_RES_PENDING);
    bte_handle_events();
    /* The connection should not be complete */
    ASSERT_TRUE(replies.empty());

    /* Send an error response */
    sendConnectResponse(reqId, BTE_L2CAP_CONN_RESP_RES_ERR_SECBLOCK);
    bte_handle_events();

    /* Now the connection should be open */
    std::vector<BteL2capNewConfiguredReply> expectedReplies = {
        {BTE_L2CAP_CONN_RESP_RES_ERR_SECBLOCK},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capNewConfigured, testConfigurationError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capNewConfiguredReply> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2capOpt,
                           const BteL2capNewConfiguredReply &reply) {
        ASSERT_FALSE(l2capOpt.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::ConfigureParams conf;
    conf.setMtu(800);
    L::newConfigured(m_client, address, psm, {}, 0, conf, onConnected);

    /* This is the HCI connect request */
    ASSERT_EQ(m_backend.sentCommands().size(), 1);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address, m_connHandle);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    sendConnectResponse(reqId);
    bte_handle_events();

    /* Read the configure request */
    reqId = m_cmdId++;
    Buffer confRaw {
        L2CAP_CONFIG_MTU, 2, low(800), high(800),
    };
    expectedData = makeConfigRequest(confRaw, reqId, m_remoteCid);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send an error configure reply */
    Buffer confRej {
        L2CAP_CONFIG_MTU, 2, low(800), high(800),
    };
    sendConfigResponse(confRej, reqId, L2CAP_CONFIG_RES_ERR_UNKNOWN);
    bte_handle_events();

    std::vector<BteL2capNewConfiguredReply> expectedReplies = {
        {BTE_L2CAP_CONN_RESP_RES_CONFIG},
    };
    ASSERT_EQ(replies, expectedReplies);

    /* Verify that we asked for disconnection */
    reqId = m_cmdId++;
    expectedData = makeDisconnectRequest(reqId, m_remoteCid, m_localCid);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    sendDisconnectResponse(reqId);
    bte_handle_events();
}
