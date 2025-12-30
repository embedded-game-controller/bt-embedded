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

class TestL2capConfig: public testing::Test
{
protected:
    TestL2capConfig() {
        dummy_driver_set_acl_limits(64, 2);
        BteBdAddr address = {1, 2, 3, 4, 5, 6};
        auto onConnected = [this](std::optional<Bte::L2cap> l2cap,
                                  const BteL2capConnectionResponse &reply) {
            ASSERT_TRUE(l2cap.has_value());
            m_l2cap.reset(new Bte::L2cap(l2cap.value()));
        };
        Bte::L2cap::newOutgoing(m_client.hci(), address, BTE_L2CAP_PSM_SDP,
                                {}, onConnected);
        /* Send the statue reply for HCI create connection */
        uint8_t status = 0;
        m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
        bte_handle_events();
        /* Send the actual reply */
        const uint8_t eventSize = 1 + 2 + 6 + 1 + 1;
        uint8_t link_type = 1;
        uint8_t enc_mode = 0;
        m_backend.sendEvent(
            Buffer{HCI_CONNECTION_COMPLETE, eventSize, status, 0x00, 0x01} +
            address + Buffer{link_type, enc_mode});
        bte_handle_events();

        /* Read the L2CAP connection request */
        Buffer conf = m_backend.lastData();

        /* Send the L2cap connect response */
        m_cmdId = 1; /* TODO: get it from the request */
        m_backend.sendData(Buffer{
            0x00, 0x01, /* Connection handle */
            16, 0, /* Total length */
            12, 0, /* L2CAP length */
            0x01, 0x00, /* Signalling channel */
            L2CAP_SIGNAL_CONN_RSP,
            m_cmdId++,
            8, 0, /* command length */
            0x40, 0x00, /* dest CID */
            0x40, 0x00, /* source CID */
            0x00, 0x00, /* successful result */
            0x00, 0x00, /* status (no more info) */
            });
        bte_handle_events();
    }

    void SetUp() override {
        m_backend.clear();
        dummy_driver_set_acl_limits(600, 2);
        setRemoteMtu(600);
    }

    void TearDown() override {
    }

    void setRemoteMtu(uint16_t mtu) {
        m_l2cap->m_l2cap->remote_mtu = mtu;
    }

    static uint8_t low(uint16_t v) {
        return v & 0xff;
    }

    static uint8_t high(uint16_t v) {
        return v >> 8;
    }

    Buffer makeResponse(const Buffer &config, uint8_t reqId,
                        uint16_t result = L2CAP_CONFIG_RES_OK,
                        bool continuation = false) {
        uint8_t confSize = config.size();
        uint16_t flags = continuation ? L2CAP_CONFIG_FLAG_CONTINUATION : 0;
        return Buffer{
            0x00, 0x21, /* Connection handle */
            low(14 + confSize), high(14 + confSize), /* Total length */
            low(10 + confSize), high(10 + confSize), /* L2CAP length */
            0x01, 0x00, /* Signalling channel */
            L2CAP_SIGNAL_CONFIG_RSP,
            reqId,
            low(6 + confSize), high(6 + confSize), /* command length */
            0x40, 0x00, /* source CID */
            low(flags), high(flags), /* flags */
            low(result), high(result), /* result */
            } + config;
    }

    void peerResponds(const Buffer &config, uint8_t reqId,
                      uint16_t result = L2CAP_CONFIG_RES_OK,
                      bool continuation = false) {
        m_backend.sendData(makeResponse(config, reqId, result, continuation));
    }

    Buffer makeRequest(const Buffer &config, uint8_t reqId,
                       bool continuation = false) {
        uint8_t confSize = config.size();
        uint16_t flags = continuation ? L2CAP_CONFIG_FLAG_CONTINUATION : 0;
        return Buffer{
            0x00, 0x21, /* 0x100 handle + flushable flag */
            low(12 + confSize), high(12 + confSize), /* Total length */
            low(8 + confSize), high(8 + confSize), /* L2CAP length */
            0x01, 0x00, /* signalling channel */
            L2CAP_SIGNAL_CONFIG_REQ,
            reqId,
            low(4 + confSize), high(4 + confSize), /* cmd length */
            0x40, 00, /* CID */
            low(flags), high(flags), /* flags (continuation) */
        } + config;
    }

    void sendRequest(const Buffer &config, uint8_t reqId,
                     bool continuation = false) {
        m_backend.sendData(makeRequest(config, reqId, continuation));
    }

    MockBackend m_backend;
    Bte::Client m_client;
    std::unique_ptr<Bte::L2cap> m_l2cap;
    uint8_t m_cmdId;
};

TEST_F(TestL2capConfig, testOutgoingEmpty) {
    using L = Bte::L2cap;
    L::ConfigureParams params;
    std::vector<L::ConfigureReply> replies;
    m_l2cap->configure(params, [&](const L::ConfigureReply &r) {
        replies.push_back(r);
    });

    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeRequest({}, reqId);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send an empty reply */
    peerResponds(Buffer(), reqId);
    bte_handle_events();

    std::vector<L::ConfigureReply> expectedReplies = {
        {},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConfig, testOutgoingSingleFields) {
    using L = Bte::L2cap;
    L::ConfigureParams params;
    params.setMtu(0x1234);
    params.setFlushTimeout(0x2345);
    params.setFrameCheckSequence(0x67);
    params.setMaxWindowSize(0x3456);
    std::vector<L::ConfigureReply> replies;
    m_l2cap->configure(params, [&](const L::ConfigureReply &r) {
        replies.push_back(r);
    });

    uint8_t reqId = m_cmdId++;
    Buffer config {
        L2CAP_CONFIG_MTU, 2, 0x34, 0x12,
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x45, 0x23,
        L2CAP_CONFIG_FRAME_CHECK_SEQ, 1, 0x67,
        L2CAP_CONFIG_MAX_WINDOW_SIZE, 2, 0x56, 0x34,
    };
    Buffer expectedData = makeRequest(config, reqId);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send a reply confirming all these fields */
    peerResponds(config, reqId);
    bte_handle_events();

    std::vector<L::ConfigureReply> expectedReplies = {
        {0, 0, params},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConfig, testOutgoingCompositeFields) {
    using L = Bte::L2cap;
    L::ConfigureParams params;
    params.setQos({
        0x11,
        0x22,
        0x33445566,
        0x77889911,
        0x22334455,
        0x66778899,
        0x11223344,
    });
    params.setRetxFlow({
        0x11,
        0x22,
        0x33,
        0x4455,
        0x6677,
        0x8899,
    });
    params.setExtFlow({
        0x11,
        0x22,
        0x3344,
        0x55667788,
        0x99112233,
        0x44556677,
    });
    std::vector<L::ConfigureReply> replies;
    m_l2cap->configure(params, [&](const L::ConfigureReply &r) {
        replies.push_back(r);
    });

    uint8_t reqId = m_cmdId++;
    uint8_t qosSize = 2 + 5 * 4;
    uint8_t retxFlowSize = 3 + 3 * 2;
    uint8_t extFlowSize = 2 + 1 * 2 + 3 * 4;
    Buffer config {
        L2CAP_CONFIG_QOS, qosSize, 0x11, 0x22,
        0x66, 0x55, 0x44, 0x33,
        0x11, 0x99, 0x88, 0x77,
        0x55, 0x44, 0x33, 0x22,
        0x99, 0x88, 0x77, 0x66,
        0x44, 0x33, 0x22, 0x11,
        L2CAP_CONFIG_RETX_FLOW, retxFlowSize, 0x11, 0x22, 0x33,
        0x55, 0x44,
        0x77, 0x66,
        0x99, 0x88,
        L2CAP_CONFIG_EXT_FLOW, extFlowSize, 0x11, 0x22,
        0x44, 0x33,
        0x88, 0x77, 0x66, 0x55,
        0x33, 0x22, 0x11, 0x99,
        0x77, 0x66, 0x55, 0x44,
    };
    Buffer expectedData = makeRequest(config, reqId);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send a reply containing the same fields */
    peerResponds(config, reqId);
    bte_handle_events();

    std::vector<L::ConfigureReply> expectedReplies = {
        {0, 0, params},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConfig, testFragmentation) {
    setRemoteMtu(48);
    using L = Bte::L2cap;
    L::ConfigureParams params;
    params.setMtu(0x1234);
    params.setFlushTimeout(0x2345);
    params.setFrameCheckSequence(0x67);
    params.setMaxWindowSize(0x3456);
    params.setQos({
        0x11,
        0x22,
        0x33445566,
        0x77889911,
        0x22334455,
        0x66778899,
        0x11223344,
    });
    params.setRetxFlow({
        0x11,
        0x22,
        0x33,
        0x4455,
        0x6677,
        0x8899,
    });
    params.setExtFlow({
        0x11,
        0x22,
        0x3344,
        0x55667788,
        0x99112233,
        0x44556677,
    });
    std::vector<L::ConfigureReply> replies;
    m_l2cap->configure(params, [&](const L::ConfigureReply &r) {
        replies.push_back(r);
    });

    uint8_t reqId0 = m_cmdId++;
    uint8_t reqId1 = m_cmdId++;
    uint8_t qosSize = 2 + 5 * 4;
    uint8_t retxFlowSize = 3 + 3 * 2;
    uint8_t extFlowSize = 2 + 1 * 2 + 3 * 4;
    Buffer config0 {
        L2CAP_CONFIG_MTU, 2, 0x34, 0x12,
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x45, 0x23,
        L2CAP_CONFIG_QOS, qosSize, 0x11, 0x22,
        0x66, 0x55, 0x44, 0x33,
        0x11, 0x99, 0x88, 0x77,
        0x55, 0x44, 0x33, 0x22,
        0x99, 0x88, 0x77, 0x66,
        0x44, 0x33, 0x22, 0x11,
        L2CAP_CONFIG_RETX_FLOW, retxFlowSize, 0x11, 0x22, 0x33,
        0x55, 0x44,
        0x77, 0x66,
        0x99, 0x88,
        L2CAP_CONFIG_FRAME_CHECK_SEQ, 1, 0x67,
    };
    Buffer config1 {
        L2CAP_CONFIG_EXT_FLOW, extFlowSize, 0x11, 0x22,
        0x44, 0x33,
        0x88, 0x77, 0x66, 0x55,
        0x33, 0x22, 0x11, 0x99,
        0x77, 0x66, 0x55, 0x44,
        L2CAP_CONFIG_MAX_WINDOW_SIZE, 2, 0x56, 0x34,
    };
    std::vector<Buffer> expectedData = {
        makeRequest(config0, reqId0, true),
        makeRequest(config1, reqId1, false),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
    m_backend.clear();

    /* Send a reply containing the same fields, but split differently;
     * first, send an ack reply to the first packet: */
    peerResponds({}, reqId0, L2CAP_CONFIG_RES_OK, true);
    bte_handle_events();

    /* Then send a couple of rejected parameters */
    Buffer configRej {
        L2CAP_CONFIG_MTU, 2, 0x01, 0x23,
        L2CAP_CONFIG_RETX_FLOW, retxFlowSize, 0xAA, 0xBB, 0xCC,
        0x11, 0x22,
        0x33, 0x44,
        0x55, 0x66,
    };
    peerResponds(configRej, reqId1, L2CAP_CONFIG_RES_ERR_PARAMS, true);
    bte_handle_events();

    /* At this point we should have sent another null-option packet to prompt
     * the peer to continue sending the configuration response */
    uint8_t reqId2 = m_cmdId++;
    expectedData = {
        makeRequest({}, reqId2),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);

    /* And to this we reply with some confirmed fields */
    Buffer configOk {
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x45, 0x23,
        L2CAP_CONFIG_QOS, qosSize, 0x11, 0x22,
        0x66, 0x55, 0x44, 0x33,
        0x11, 0x99, 0x88, 0x77,
        0x55, 0x44, 0x33, 0x22,
        0x99, 0x88, 0x77, 0x66,
        0x44, 0x33, 0x22, 0x11,
    };
    peerResponds(configOk, reqId2);
    bte_handle_events();

    L::ConfigureParams expectedParams;
    expectedParams.setMtu(0x2301);
    expectedParams.setFlushTimeout(0x2345);
    expectedParams.setQos({
        0x11,
        0x22,
        0x33445566,
        0x77889911,
        0x22334455,
        0x66778899,
        0x11223344,
    });
    expectedParams.setRetxFlow({
        0xaa,
        0xbb,
        0xcc,
        0x2211,
        0x4433,
        0x6655,
    });
    std::vector<L::ConfigureReply> expectedReplies = {
        {BTE_L2CAP_CONFIG_MTU|BTE_L2CAP_CONFIG_RETX_FLOW, 0, expectedParams},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConfig, testOutgoingUnknownParam) {
    using L = Bte::L2cap;
    L::ConfigureParams params;
    params.setMtu(0x1234);
    params.setFlushTimeout(0x2345);
    params.setFrameCheckSequence(0x67);
    std::vector<L::ConfigureReply> replies;
    m_l2cap->configure(params, [&](const L::ConfigureReply &r) {
        replies.push_back(r);
    });

    uint8_t reqId = m_cmdId++;
    Buffer config {
        L2CAP_CONFIG_MTU, 2, 0x34, 0x12,
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x45, 0x23,
        L2CAP_CONFIG_FRAME_CHECK_SEQ, 1, 0x67,
    };
    Buffer expectedData = makeRequest(config, reqId);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send a reply pretending that flush timeout is unknown */
    Buffer configUnknown {
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x45, 0x23,
    };
    peerResponds(configUnknown, reqId, L2CAP_CONFIG_RES_ERR_UNKNOWN, false);
    bte_handle_events();

    std::vector<L::ConfigureReply> expectedReplies = {
        {0, BTE_L2CAP_CONFIG_FLUSH_TIMEOUT, {}},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST_F(TestL2capConfig, testIncomingEmpty) {
    using L = Bte::L2cap;
    std::vector<L::ConfigureParams> incomingParams;
    m_l2cap->onConfigureRequest([&](const L::ConfigureParams &params) {
        incomingParams.push_back(params);
        m_l2cap->setConfigureReply({});
    });

    uint8_t reqId = 56;
    /* Send an empty request */
    sendRequest({}, reqId);
    bte_handle_events();

    std::vector<L::ConfigureParams> expectedParams = {
        {},
    };
    ASSERT_EQ(incomingParams, expectedParams);

    /* Check that our reply was sent */
    std::vector<Buffer> expectedData = {
        makeResponse({}, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capConfig, testIncomingUnknownParam) {
    using L = Bte::L2cap;
    std::vector<L::ConfigureParams> incomingParams;
    m_l2cap->onConfigureRequest([&](const L::ConfigureParams &params) {
        incomingParams.push_back(params);
    });

    uint8_t reqId = 56;
    /* Send a request with an unrecognized parameter */
    Buffer config {
        0x77, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x11, 0x22,
    };
    sendRequest(config, reqId);
    bte_handle_events();

    std::vector<L::ConfigureParams> expectedParams = {};
    ASSERT_EQ(incomingParams, expectedParams);

    /* Check that our reply was sent */
    Buffer configErr {
        0x77, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    };
    std::vector<Buffer> expectedData = {
        makeResponse(configErr, reqId, L2CAP_CONFIG_RES_ERR_UNKNOWN),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capConfig, testIncomingUnknownParamHint) {
    using L = Bte::L2cap;
    std::vector<L::ConfigureParams> incomingParams;
    m_l2cap->onConfigureRequest([&](const L::ConfigureParams &params) {
        incomingParams.push_back(params);
    });

    uint8_t reqId = 56;
    /* Send a request with an unrecognized parameter */
    Buffer config {
        0xAA, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x11, 0x22,
    };
    sendRequest(config, reqId);
    bte_handle_events();

    L::ConfigureParams params;
    params.setFlushTimeout(0x2211);
    std::vector<L::ConfigureParams> expectedParams = {
        params,
    };
    ASSERT_EQ(incomingParams, expectedParams);

    /* Check that our reply was sent */
    Buffer configOk {
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x11, 0x22,
    };
    std::vector<Buffer> expectedData = {
        makeResponse(configOk, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capConfig, testIncomingWithLongerResponse) {
    using L = Bte::L2cap;
    std::vector<L::ConfigureParams> incomingParams;
    m_l2cap->onConfigureRequest([&](const L::ConfigureParams &params) {
        incomingParams.push_back(params);
        /* Set a small MTU, to ensure that our response gets fragmented */
        setRemoteMtu(30);
        /* Reject all parameters, alter them slightly */
        L::ConfigureParams newParams;
        BteL2capConfigQos qos = params.qos();
        qos.token_rate += 16;
        newParams.setQos(qos);
        BteL2capConfigRetxFlow retxFlow = params.retxFlow();
        retxFlow.monitor_timeout += 16;
        newParams.setRetxFlow(retxFlow);
        newParams.setFlushTimeout(params.flushTimeout() + 16);
        L::ConfigureReply reply = {
            params.p.field_mask, /* rejected */
            0, /* unknown */
            newParams,
        };
        m_l2cap->setConfigureReply(reply);
    });

    uint8_t reqId = 42;
    /* Send a request with a couple of parameters */
    uint8_t qosSize = 2 + 5 * 4;
    uint8_t retxFlowSize = 3 + 3 * 2;
    Buffer config {
        L2CAP_CONFIG_QOS, qosSize, 0x11, 0x22,
        0x66, 0x55, 0x44, 0x33,
        0x11, 0x99, 0x88, 0x77,
        0x55, 0x44, 0x33, 0x22,
        0x99, 0x88, 0x77, 0x66,
        0x44, 0x33, 0x22, 0x11,
        L2CAP_CONFIG_RETX_FLOW, retxFlowSize, 0x11, 0x22, 0x33,
        0x55, 0x44,
        0x77, 0x66,
        0x99, 0x88,
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x11, 0x22,
    };
    sendRequest(config, reqId);
    bte_handle_events();

    ASSERT_EQ(incomingParams.size(), 1);

    /* Check that our reply was sent */
    Buffer config0 {
        L2CAP_CONFIG_FLUSH_TIMEOUT, 2, 0x21, 0x22,
        L2CAP_CONFIG_QOS, qosSize, 0x11, 0x22,
        0x76, 0x55, 0x44, 0x33,
        0x11, 0x99, 0x88, 0x77,
        0x55, 0x44, 0x33, 0x22,
        0x99, 0x88, 0x77, 0x66,
        0x44, 0x33, 0x22, 0x11,
    };
    std::vector<Buffer> expectedData = {
        makeResponse(config0, reqId, L2CAP_CONFIG_RES_ERR_REJ, true),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
    m_backend.clear();

    /* Now the client sends an empty request to retrieve the rest of our
     * response */
    uint8_t reqId2 = 45;
    sendRequest({}, reqId2);
    bte_handle_events();

    /* Check that the rest of our reply was sent */
    Buffer config1 {
        L2CAP_CONFIG_RETX_FLOW, retxFlowSize, 0x11, 0x22, 0x33,
        0x55, 0x44,
        0x87, 0x66,
        0x99, 0x88,
    };
    expectedData = {
        makeResponse(config1, reqId2, L2CAP_CONFIG_RES_ERR_REJ, false),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capConfig, testConfigureMoreData) {
    using L = Bte::L2cap;
    L::ConfigureParams params;
    m_l2cap->configure(params, [&](const L::ConfigureReply &r) {});

    uint8_t reqId = m_cmdId++;
    Buffer expectedData = makeRequest({}, reqId);
    ASSERT_EQ(m_backend.lastData(), expectedData);

    /* Send an empty reply, sending the extra field was crashing the lib during
     * development, so this test verifies that nothing bad happens if the peer
     * sends us more data */
    m_backend.sendData(Buffer{
        0x00, 0x01, /* Connection handle */
        16, 0, /* Total length */
        12, 0, /* L2CAP length */
        0x01, 0x00, /* Signalling channel */
        L2CAP_SIGNAL_CONFIG_RSP,
        reqId,
        8, 0, /* command length */
        0x40, 0x00, /* source CID */
        0x40, 0x00, /* extra field */
        0x00, 0x00, /* flags */
        0x00, 0x00, /* result (success) */
        });
    bte_handle_events();
}
