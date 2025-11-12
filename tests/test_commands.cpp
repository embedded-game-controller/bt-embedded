#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"
#include <gtest/gtest.h>
#include <iostream>

Buffer createInquiryResult(const BteBdAddr &address)
{
    const uint8_t *b = address.bytes;
    return Buffer {
        HCI_INQUIRY_RESULT,
        15, // len
        1, // num responses
        b[0], b[1], b[2], b[3], b[4], b[5],
        0, // scan rep
        0, // scan period
        0, // reserved
        0, 0, 0, // device class
        0, 0, // clock offset
    };
}

TEST(Commands, Inquiry) {
    MockBackend backend;

    BteClient *client = bte_client_new();
    BteHci *hci = bte_hci_get(client);

    static std::vector<BteHciReply> statusCalls;
    static std::vector<StoredInquiryReply> inquiryCalls;
    struct Callbacks {
        static void statusCb(BteHci *hci, const BteHciReply *reply,
                             void *userdata) {
            statusCalls.push_back(*reply);
        }
        static void inquiryCb(BteHci *hci, const BteHciInquiryReply *reply,
                              void *userdata) {
            inquiryCalls.push_back(*reply);
        }
    };

    uint32_t requestedLap = 0xaabbcc;
    uint8_t requestedLen = 4;
    uint8_t requestedMaxResp = 9;

    bte_hci_inquiry(hci, requestedLap, requestedLen, requestedMaxResp,
                    &Callbacks::statusCb, &Callbacks::inquiryCb);

    /* Verify that the expected command was sent */
    Buffer expectedCommand {
        0x01, 0x04, 5, 0xcc, 0xbb, 0xaa, requestedLen, requestedMaxResp
    };
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    /* Send a status event */
    uint8_t status = 0;
    backend.sendEvent({
        HCI_COMMAND_STATUS, 4,
        status,
        1, // packets
        0x01, 0x04, // opcode
    });
    bte_handle_events();
    std::vector<BteHciReply> expectedStatusCalls = {{status}};
    ASSERT_EQ(statusCalls, expectedStatusCalls);

    /* Emit the Inquiry Result events */
    std::vector<BteBdAddr> addresses;
    for (uint8_t i = 0; i < 50; i++) {
        addresses.emplace_back(BteBdAddr{ 1, 2, 3, 4, 5, uint8_t(6 + i)});
    };
    for (const BteBdAddr &address: addresses) {
        backend.sendEvent(createInquiryResult(address));
    }

    /* And an inquiry complete */
    backend.sendEvent({ HCI_INQUIRY_COMPLETE, 1, 0 });
    bte_handle_events();

    /* Verify that our callback has been invoked */
    ASSERT_EQ(inquiryCalls.size(), 1);
    const StoredInquiryReply &reply = inquiryCalls[0];
    ASSERT_EQ(reply.num_responses, addresses.size());

    for (uint8_t i = 0; i < addresses.size(); i++) {
        ASSERT_EQ(reply.responses[i].address, addresses[i]);
    };
    bte_client_unref(client);
}

TEST(Commands, InquiryFailed) {
    MockBackend backend;

    BteClient *client = bte_client_new();
    BteHci *hci = bte_hci_get(client);

    static std::vector<BteHciReply> statusCalls;
    static std::vector<StoredInquiryReply> inquiryCalls;
    struct Callbacks {
        static void statusCb(BteHci *hci, const BteHciReply *reply,
                             void *userdata) {
            statusCalls.push_back(*reply);
        }
        static void inquiryCb(BteHci *hci, const BteHciInquiryReply *reply,
                              void *userdata) {
            inquiryCalls.push_back(*reply);
        }
    };

    uint32_t requestedLap = 0xaabbcc;
    uint8_t requestedLen = 4;
    uint8_t requestedMaxResp = 9;

    bte_hci_inquiry(hci, requestedLap, requestedLen, requestedMaxResp,
                    &Callbacks::statusCb, &Callbacks::inquiryCb);

    /* Send a status event */
    uint8_t status = HCI_HW_FAILURE;
    backend.sendEvent({
        HCI_COMMAND_STATUS, 4,
        status,
        1, // packets
        0x01, 0x04, // opcode
    });
    bte_handle_events();
    std::vector<BteHciReply> expectedStatusCalls = {{status}};
    ASSERT_EQ(statusCalls, expectedStatusCalls);

    /* Verify that our callback has not been invoked */
    ASSERT_EQ(inquiryCalls.size(), 0);
    bte_client_unref(client);
}
