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

class TestMinimalL2capEcho: public TestL2capFixtureConnected
{
};

TEST_F(TestMinimalL2capEcho, testAutomaticReceive) {
    Buffer data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    uint8_t reqId = 78;
    sendEchoRequest(data, reqId);
    bte_handle_events();

    /* Verify that our response is as expected */
    std::vector<Buffer> expectedData = {
        makeEchoResponse({}, reqId),
    };
    ASSERT_EQ(m_backend.sentData(), expectedData);
}
