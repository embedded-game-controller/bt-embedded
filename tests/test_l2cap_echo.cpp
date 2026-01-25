#include "l2cap_fixtures.h"
#include "dummy_driver.h"
#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/l2cap_proto.h"
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

class TestL2capEcho: public TestL2capFixtureConnected
{
};

TEST_F(TestL2capEcho, testSend) {
    Buffer data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::vector<Buffer> receivedData;
    bool ok = m_l2cap->echo(data, [&](BufferList::Reader &reader) {
        receivedData.push_back(reader.readAll());
    });
    ASSERT_TRUE(ok);
    uint8_t reqId = m_cmdId++;

    /* Verify that our request is as expected */
    std::vector<Buffer> expectedData = {
        makeEchoRequest(data, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
    ASSERT_TRUE(receivedData.empty());

    /* Now send a reply */
    Buffer replyData = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255 };
    sendEchoResponse(replyData, reqId);
    bte_handle_events();
    expectedData = {
        replyData,
    };
    ASSERT_EQ(receivedData, expectedData);
}

TEST_F(TestL2capEcho, testReceive) {
    std::vector<Buffer> receivedData;
    m_l2cap->onEcho([&](BufferList::Reader &reader,
                        BufferList::Writer *writer) {
        Buffer data = reader.readAll();
        receivedData.push_back(data);
        if (writer) {
            std::reverse(data.begin(), data.end());
            writer->write(data + data);
        }
        return uint16_t(data.size() * 2);
    });

    Buffer data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    uint8_t reqId = 78;
    sendEchoRequest(data, reqId);
    bte_handle_events();

    /* Verify that we received the request (twice) */
    std::vector<Buffer> expectedData = {
        data, data
    };
    ASSERT_EQ(receivedData, expectedData);

    /* Verify that our response is as expected */
    Buffer replyData = {
        9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
        9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    };
    expectedData = {
        makeEchoResponse(replyData, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}
