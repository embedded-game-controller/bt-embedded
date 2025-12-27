#include "bte_l2cap_cpp.h"
#include "dummy_driver.h"
#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/l2cap_proto.h"
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

class TestL2capConnect: public testing::Test
{
protected:
    TestL2capConnect() {
    }

    void SetUp() override {
        m_backend.clear();
        dummy_driver_set_acl_limits(600, 2);
        setLocalCid(0x0040);
        setRemoteCid(0x0040);
        m_cmdId = 1;
    }

    void TearDown() override {
    }

    void setLocalCid(BteL2capChannelId cid) {
        m_localCid = cid;
    }

    void setRemoteCid(BteL2capChannelId cid) {
        m_remoteCid = cid;
    }

    static uint8_t low(uint16_t v) {
        return v & 0xff;
    }

    static uint8_t high(uint16_t v) {
        return v >> 8;
    }

    Buffer makeResponse(uint8_t reqId,
                        uint16_t result = BTE_L2CAP_CONN_RESP_RES_OK,
                        uint16_t status = BTE_L2CAP_CONN_RESP_STATUS_NO_INFO) {
        return Buffer{
            0x00, 0x21, /* Connection handle */
            16, 0, /* Total length */
            12, 0, /* L2CAP length */
            0x01, 0x00, /* Signalling channel */
            L2CAP_SIGNAL_CONN_RSP,
            reqId,
            8, 0, /* command length */
            low(m_remoteCid), high(m_remoteCid), /* dest CID */
            low(m_localCid), high(m_localCid), /* source CID */
            low(result), high(result),
            low(status), high(status),
        };
    }

    void peerResponds(uint8_t reqId,
                      uint16_t result = BTE_L2CAP_CONN_RESP_RES_OK,
                      uint16_t status = BTE_L2CAP_CONN_RESP_STATUS_NO_INFO) {
        m_backend.sendData(makeResponse(reqId, result, status));
    }

    Buffer makeRequest(uint8_t reqId, BteL2capPsm psm) {
        return Buffer{
            0x00, 0x21, /* 0x100 handle + flushable flag */
            12, 0, /* Total length */
            8, 0, /* L2CAP length */
            0x01, 0x00, /* signalling channel */
            L2CAP_SIGNAL_CONN_REQ,
            reqId,
            4, 0, /* cmd length */
            low(psm), high(psm),
            low(m_localCid), high(m_localCid), /* source CID */
        };
    }

    void sendRequest(uint8_t reqId, BteL2capPsm psm) {
        m_backend.sendData(makeRequest(reqId, psm));
    }

    Buffer makeHciCreateConnection(const BteBdAddr &address,
                                   BtePacketType packetType,
                                   uint8_t pageScanRepMode,
                                   uint16_t clockOffset,
                                   bool allowRoleSwitch) {
        uint8_t size = 13;
        const uint8_t *b = address.bytes;
        return {
            0x5, 0x4, size, b[0], b[1], b[2], b[3], b[4], b[5],
            low(packetType), high(packetType), pageScanRepMode,
            0, low(clockOffset), high(clockOffset), allowRoleSwitch
        };
    }

    void sendHciConnectionComplete(const BteBdAddr &address) {
        const uint8_t eventSize = 1 + 2 + 6 + 1 + 1;
        uint8_t link_type = 1; /* ACL */
        uint8_t enc_mode = 0;
        uint8_t status = 0;
        m_backend.sendEvent(
            Buffer{HCI_CONNECTION_COMPLETE, eventSize, status, 0x00, 0x01} +
            address + Buffer{link_type, enc_mode});
    }

    MockBackend m_backend;
    Bte::Client m_client;
    BteL2capChannelId m_localCid;
    BteL2capChannelId m_remoteCid;
    uint8_t m_cmdId;
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
    L::connect(m_client.hci(), address, psm, {}, onConnected);

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
    Buffer expectedData = makeRequest(reqId, psm);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send the L2cap connect response */
    peerResponds(reqId);
    bte_handle_events();

    std::vector<BteL2capConnectionResponse> expectedReplies = {
        {0x40, 0x40, 0, 0},
    };
    ASSERT_EQ(replies, expectedReplies);
}
