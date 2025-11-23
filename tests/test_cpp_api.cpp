#include "bte_cpp.h"
#include "mock_backend.h"
#include "type_utils.h"

#include <gtest/gtest.h>

TEST(CppAPI, testConcurrentRequests)
{
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    /* The first int is the index of the hci instance */
    using Reply = std::tuple<BteHciReply,int>;
    std::vector<Reply> replies;
    hci.setEventMask(0, [&](const BteHciReply &reply) {
        replies.push_back({reply, 1});
    });
    hci.reset([&](const BteHciReply &reply) {
        replies.push_back({reply, 2});
    });
    hci.writePinType(BTE_HCI_PIN_TYPE_FIXED, [&](const BteHciReply &reply) {
        replies.push_back({reply, 3});
    });

    /*  We don't check the sent data here, there are other tests for this. We
     *  only send the reply and verify that the right callbacks are called. */
    uint8_t status0 = 6;
    uint8_t status1 = 5;
    uint8_t status2 = 4;
    backend.sendEvent({ HCI_COMMAND_COMPLETE, 4, 1, 0x3, 0xc, status0 });
    backend.sendEvent({ HCI_COMMAND_COMPLETE, 4, 1, 0xa, 0xc, status1 });
    backend.sendEvent({ HCI_COMMAND_COMPLETE, 4, 1, 0x1, 0xc, status2 });
    bte_handle_events();

    std::vector<Reply> expectedReplies = {
        {{status0}, 2},
        {{status1}, 3},
        {{status2}, 1},
    };
    ASSERT_EQ(replies, expectedReplies);
}
