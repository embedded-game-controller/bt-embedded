#include "l2cap_fixtures.h"

#include <gtest/gtest.h>
#include <iostream>

TEST_F(TestL2capFixtureConnected, testWeDisconnect) {
    std::vector<uint8_t> disconnectReasons;
    m_l2cap->onDisconnected([&](uint8_t reason) {
        disconnectReasons.push_back(reason);
    });
    BteL2capState state = m_l2cap->state();
    m_l2cap->onStateChanged([&](BteL2capState s) {
        state = s;
    });
    ASSERT_EQ(state, BTE_L2CAP_WAIT_CONFIG);
    m_l2cap->disconnect();

    ASSERT_EQ(state, BTE_L2CAP_WAIT_DISCONNECT);

    /* Verify that our request is as expected */
    uint8_t reqId = m_cmdId++;
    std::vector<Buffer> expectedData {
        makeDisconnectRequest(reqId, m_remoteCid, m_localCid),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);

    sendDisconnectResponse(reqId);
    bte_handle_events();
    std::vector<uint8_t> expectedReasons {
        BTE_HCI_CONN_TERMINATED_BY_LOCAL_HOST,
    };
    ASSERT_EQ(disconnectReasons, expectedReasons);
    ASSERT_EQ(state, BTE_L2CAP_CLOSED);
}

TEST_F(TestL2capFixtureConnected, testRemoteDisconnects) {
    std::vector<uint8_t> disconnectReasons;
    m_l2cap->onDisconnected([&](uint8_t reason) {
        disconnectReasons.push_back(reason);
    });
    BteL2capState state = m_l2cap->state();
    m_l2cap->onStateChanged([&](BteL2capState s) {
        state = s;
    });
    ASSERT_EQ(state, BTE_L2CAP_WAIT_CONFIG);

    /* Let remote send a disconnect request */
    uint8_t incomingReqId = 123;
    sendDisconnectRequest(incomingReqId);
    bte_handle_events();

    std::vector<uint8_t> expectedReasons {
        BTE_HCI_OTHER_END_CLOSED_CONN_USER,
    };
    ASSERT_EQ(disconnectReasons, expectedReasons);
    ASSERT_EQ(state, BTE_L2CAP_CLOSED);

    /* Verify that we send a proper response */
    std::vector<Buffer> expectedData {
        makeDisconnectResponse(incomingReqId, m_localCid, m_remoteCid),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}
