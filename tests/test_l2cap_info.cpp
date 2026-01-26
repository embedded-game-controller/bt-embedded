#include "l2cap_fixtures.h"
#include "dummy_driver.h"
#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/l2cap_proto.h"
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

class TestL2capInfo: public TestL2capFixtureConnected
{
};

TEST_F(TestL2capInfo, testOutgoingMtu) {
    std::vector<BteL2capInfo> receivedInfo;
    bool ok = m_l2cap->queryInfo(BTE_L2CAP_INFO_TYPE_MTU,
                                 [&](const BteL2capInfo &info) {
        receivedInfo.push_back(info);
    });
    ASSERT_TRUE(ok);
    uint8_t reqId = m_cmdId++;

    /* Verify that our request is as expected */
    std::vector<Buffer> expectedData = {
        makeInfoRequest(BTE_L2CAP_INFO_TYPE_MTU, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
    ASSERT_TRUE(receivedInfo.empty());

    /* Now send a reply */
    sendInfoResponse(reqId, BTE_L2CAP_INFO_TYPE_MTU,
                     BTE_L2CAP_INFO_RESP_RES_OK, uint16_t(67));
    bte_handle_events();
    std::vector<BteL2capInfo> expectedInfo = {
        BteL2capInfo {
            BTE_L2CAP_INFO_TYPE_MTU,
            BTE_L2CAP_INFO_RESP_RES_OK,
            {
                .connectionless_mtu = 67
            },
        },
    };
    ASSERT_EQ(receivedInfo, expectedInfo);
}

TEST_F(TestL2capInfo, testOutgoingFeatures) {
    std::vector<BteL2capInfo> receivedInfo;
    bool ok = m_l2cap->queryInfo(BTE_L2CAP_INFO_TYPE_EXT_FEATURES,
                                 [&](const BteL2capInfo &info) {
        receivedInfo.push_back(info);
    });
    ASSERT_TRUE(ok);
    uint8_t reqId = m_cmdId++;

    /* Verify that our request is as expected */
    std::vector<Buffer> expectedData = {
        makeInfoRequest(BTE_L2CAP_INFO_TYPE_EXT_FEATURES, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
    ASSERT_TRUE(receivedInfo.empty());

    /* Now send a reply */
    uint32_t features = 0x11223344;
    sendInfoResponse(reqId, BTE_L2CAP_INFO_TYPE_EXT_FEATURES,
                     BTE_L2CAP_INFO_RESP_RES_OK, features);
    bte_handle_events();
    std::vector<BteL2capInfo> expectedInfo = {
        BteL2capInfo {
            BTE_L2CAP_INFO_TYPE_EXT_FEATURES,
            BTE_L2CAP_INFO_RESP_RES_OK,
            {
                .ext_feature_mask = features
            },
        },
    };
    ASSERT_EQ(receivedInfo, expectedInfo);
}

TEST_F(TestL2capInfo, testOutgoingChannels) {
    std::vector<BteL2capInfo> receivedInfo;
    bool ok = m_l2cap->queryInfo(BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS,
                                 [&](const BteL2capInfo &info) {
        receivedInfo.push_back(info);
    });
    ASSERT_TRUE(ok);
    uint8_t reqId = m_cmdId++;

    /* Verify that our request is as expected */
    std::vector<Buffer> expectedData = {
        makeInfoRequest(BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
    ASSERT_TRUE(receivedInfo.empty());

    /* Now send a reply */
    uint64_t channels = 0x1122334455667788;
    sendInfoResponse(reqId, BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS,
                     BTE_L2CAP_INFO_RESP_RES_OK, channels);
    bte_handle_events();
    std::vector<BteL2capInfo> expectedInfo = {
        BteL2capInfo {
            BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS,
            BTE_L2CAP_INFO_RESP_RES_OK,
            {
                .fixed_channels_mask = channels
            },
        },
    };
    ASSERT_EQ(receivedInfo, expectedInfo);
}

TEST_F(TestL2capInfo, testIncomingMtu) {
    uint8_t reqId = 78;
    sendInfoRequest(BTE_L2CAP_INFO_TYPE_MTU, reqId);
    bte_handle_events();

    /* Verify that our response is as expected */
    std::vector<Buffer> expectedData = {
        makeInfoResponse(reqId, BTE_L2CAP_INFO_TYPE_MTU,
                         BTE_L2CAP_INFO_RESP_RES_OK, uint16_t(48)),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capInfo, testIncomingFeatures) {
    uint8_t reqId = 78;
    sendInfoRequest(BTE_L2CAP_INFO_TYPE_EXT_FEATURES, reqId);
    bte_handle_events();

    /* Verify that our response is as expected */
    uint32_t features = 0;
    std::vector<Buffer> expectedData = {
        makeInfoResponse(reqId, BTE_L2CAP_INFO_TYPE_EXT_FEATURES,
                         BTE_L2CAP_INFO_RESP_RES_OK, features),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capInfo, testIncomingFixedChannels) {
    uint8_t reqId = 78;
    sendInfoRequest(BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS, reqId);
    bte_handle_events();

    /* Verify that our response is as expected */
    uint64_t fixedChannels = 0x2;
    std::vector<Buffer> expectedData = {
        makeInfoResponse(reqId, BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS,
                         BTE_L2CAP_INFO_RESP_RES_OK, fixedChannels),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}

TEST_F(TestL2capInfo, testIncomingUnsupported) {
    uint8_t reqId = 78;
    sendInfoRequest(0x1234, reqId);
    bte_handle_events();

    /* Verify that our response is as expected */
    std::vector<Buffer> expectedData = {
        makeInfoResponse(reqId, 0x1234, BTE_L2CAP_INFO_RESP_RES_UNSUPPORTED),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}
