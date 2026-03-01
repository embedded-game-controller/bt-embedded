#include "l2cap_fixtures.h"

#include <gtest/gtest.h>

class TestL2capConnect: public TestL2capFixture
{
protected:
    void SetUp() override {
        TestL2capFixture::SetUp();
        m_remoteCid = 0x40;
        m_connHandle = 0x100;
    }
};

TEST_F(TestL2capConnect, testOutgoingNoHciParams) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_TRUE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newOutgoing(m_client, address, psm, {}, 0, onConnected);

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
    sendHciConnectionComplete(address);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    sendConnectResponse(reqId);
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {0x40, 0x40, 0, 0},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingWithHciParams) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_TRUE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    BtePacketType packetType = BTE_PACKET_TYPE_DH1;
    uint16_t clockOffset = 0x1234;
    uint8_t pageScanRepMode = 2;
    bool allowRoleSwitch = false;
    BteHciConnectParams params = {
        packetType,
        clockOffset,
        pageScanRepMode,
        allowRoleSwitch,
    };
    L::newOutgoing(m_client, address, psm, params, 0, onConnected);

    std::vector<Buffer> expectedCommands {
        makeHciCreateConnection(address, packetType, pageScanRepMode,
                                clockOffset, allowRoleSwitch),
    };
    ASSERT_EQ(m_backend.sentCommands(), expectedCommands);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    sendConnectResponse(reqId);
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {0x40, 0x40, 0, 0},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingWithAuthentication) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_TRUE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    BteL2CapConnectFlags flags = BTE_L2CAP_CONNECT_FLAG_AUTH;
    L::newOutgoing(m_client, address, psm, {}, flags, onConnected);

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
    sendHciConnectionComplete(address);
    bte_handle_events();

    /* We should have requested the authentication */
    expectedCommands.push_back(makeHciAuthRequested(m_connHandle));
    ASSERT_EQ(m_backend.sentCommands(), expectedCommands);

    /* Send the command status... */
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x11, 0x4});
    bte_handle_events();
    /* ..And the reply */
    sendHciAuthComplete(m_connHandle);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    sendConnectResponse(reqId);
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {0x40, 0x40, 0, 0},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingPending) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        bool isDone = reply.result != BTE_L2CAP_CONN_RESP_RES_PENDING;
        ASSERT_EQ(l2cap.has_value(), isDone);
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newOutgoing(m_client, address, psm, {}, 0, onConnected);

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
    sendHciConnectionComplete(address);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response (pending) */
    sendConnectResponse(reqId, BTE_L2CAP_CONN_RESP_RES_PENDING,
                        BTE_L2CAP_CONN_RESP_STATUS_AUTHORIZATION);
    bte_handle_events();

    /* Send the L2cap connect response */
    sendConnectResponse(reqId);
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {0x40, 0x40, BTE_L2CAP_CONN_RESP_RES_PENDING,
            BTE_L2CAP_CONN_RESP_STATUS_AUTHORIZATION},
        {0x40, 0x40, 0, 0},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingHciError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_FALSE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newOutgoing(m_client, address, psm, {}, 0, onConnected);

    /* Send the status reply for HCI create connection */
    uint8_t status = 3; /* HW failure */
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();

    /* Verify that we haven't sent anything over the data channel */
    ASSERT_TRUE(m_backend.sentData().empty());

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE,
            BTE_L2CAP_CONN_RESP_STATUS_NO_INFO
        },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingDataError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_FALSE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newOutgoing(m_client, address, psm, {}, 0, onConnected);

    int sendDataCount = 0;
    m_backend.onSendData([&](BteBuffer *) {
        sendDataCount++;
        return -1;
    });

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address);
    bte_handle_events();

    ASSERT_EQ(sendDataCount, 1);
    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE,
            BTE_L2CAP_CONN_RESP_STATUS_NO_INFO
        },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingResponseTooShortError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_FALSE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newOutgoing(m_client, address, psm, {}, 0, onConnected);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response. Make it shorter than expected */
    uint16_t result = BTE_L2CAP_CONN_RESP_RES_OK;
    uint16_t lstatus = BTE_L2CAP_CONN_RESP_STATUS_NO_INFO;
    m_backend.sendData(Buffer{
            0x00, 0x21, /* Connection handle */
            15, 0, /* Total length */
            11, 0, /* L2CAP length */
            0x01, 0x00, /* Signalling channel */
            L2CAP_SIGNAL_CONN_RSP,
            reqId,
            7, 0, /* command length */
            low(m_remoteCid), high(m_remoteCid), /* dest CID */
            low(m_localCid), high(m_localCid), /* source CID */
            low(result), high(result),
            low(lstatus), /* missing: high(lstatus), */
        });
    bte_handle_events();

    /* The response should be silenty discarded */
    ASSERT_EQ(replies.size(), 0);

    /* We need to simulate a link timeout, or the l2cap object will stay alive
     * indefinitely. */
    BteConnHandle connHandle = 0x0100;
    uint8_t hciStatus = 0;
    uint8_t reason = BTE_HCI_CONN_TIMEOUT;
    m_backend.sendEvent({ HCI_DISCONNECTION_COMPLETE, 1 + 2 + 1, hciStatus,
                        low(connHandle), high(connHandle), reason });
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE,
            BTE_L2CAP_CONN_RESP_STATUS_NO_INFO
        },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConnect, testOutgoingResponseError) {
    using L = Bte::L2cap;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteL2capConnectionResponse> replies;
    auto onConnected = [&](std::optional<Bte::L2cap> l2cap,
                           const BteL2capConnectionResponse &reply) {
        ASSERT_FALSE(l2cap.has_value());
        replies.push_back(reply);
    };
    BteL2capPsm psm = BTE_L2CAP_PSM_SDP;
    L::newOutgoing(m_client, address, psm, {}, 0, onConnected);

    /* Send the status reply for HCI create connection */
    uint8_t status = 0;
    m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();
    /* Send the actual reply */
    sendHciConnectionComplete(address);
    bte_handle_events();

    /* Read the L2CAP connection request */
    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeConnectRequest(reqId, m_connHandle, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response. Make it shorter than expected */
    sendConnectResponse(reqId, BTE_L2CAP_CONN_RESP_RES_ERR_SECBLOCK);
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CHANNEL_ID_NULL,
            BTE_L2CAP_CONN_RESP_RES_ERR_SECBLOCK,
            BTE_L2CAP_CONN_RESP_STATUS_NO_INFO
        },
    };
    ASSERT_EQ(replies, expectedReplies);
}
