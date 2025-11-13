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

struct CommandNoReplyRow {
    std::string name;
    std::function<void(BteHci *hci, BteHciDoneCb callback)> invoker;
    std::vector<uint8_t> expectedCommand;
};

class TestSyncCommandsNoReply:
    public testing::TestWithParam<CommandNoReplyRow>
{
};

TEST_P(TestSyncCommandsNoReply, testSuccessfulCommand) {
    auto params = GetParam();

    MockBackend backend;

    BteClient *client = bte_client_new();
    BteHci *hci = bte_hci_get(client);

    void *expectedUserdata = (void*)0xdeadbeef;
    bte_client_set_userdata(client, expectedUserdata);
    using StatusCall = std::tuple<BteHci *, BteHciReply, void*>;
    static std::vector<StatusCall> statusCalls;
    statusCalls.clear();
    struct Callbacks {
        static void statusCb(BteHci *hci, const BteHciReply *reply,
                             void *userdata) {
            statusCalls.push_back({hci, *reply, userdata});
        }
    };

    params.invoker(hci, &Callbacks::statusCb);

    /* Verify that the expected command was sent */
    Buffer expectedCommand{params.expectedCommand};
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    /* Obtain the opcode bytes from the sent data */
    uint8_t opcode0 = params.expectedCommand[0];
    uint8_t opcode1 = params.expectedCommand[1];
    /* Send a status event */
    uint8_t status = 0;
    backend.sendEvent({
        HCI_COMMAND_COMPLETE, 4,
        1, // packets
        opcode0, opcode1,
        status
    });
    bte_handle_events();
    std::vector<StatusCall> expectedStatusCalls = {
        {hci, {status}, expectedUserdata}
    };
    ASSERT_EQ(statusCalls, expectedStatusCalls);
    bte_client_unref(client);
}

static const std::vector<CommandNoReplyRow> s_commandsWithNoReply {
    {
        "nop",
        [](BteHci *hci, BteHciDoneCb cb) { bte_hci_nop(hci, cb); },
        {0x0, 0x0, 0}
    },
    {
        "inquiry_cancel",
        [](BteHci *hci, BteHciDoneCb cb) { bte_hci_inquiry_cancel(hci, cb); },
        {0x2, 0x4, 0}
    },
    {
        "set_event_mask",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_event_mask(hci, 0x1122334455667788, cb);
        },
        {0x1, 0xc, 8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11}
    },
    {
        "reset",
        [](BteHci *hci, BteHciDoneCb cb) { bte_hci_reset(hci, cb); },
        {0x3, 0xc, 0}
    },
    {
        "write_local_name",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_local_name(hci, "A test", cb); },
        []() {
            std::vector<uint8_t> ret{0x13, 0xc, 248, 'A', ' ', 't', 'e', 's', 't'};
            for (int i = ret.size(); i < 248 + 3; i++)
                ret.push_back(0);
            return ret;
        }()
    },
    {
        "write_class_of_device",
        [](BteHci *hci, BteHciDoneCb cb) {
            BteClassOfDevice cod{0x11, 0x22, 0x33};
            bte_hci_write_class_of_device(hci, &cod, cb); },
        {0x24, 0xc, 3, 0x11, 0x22, 0x33}
    },
};
INSTANTIATE_TEST_CASE_P(
    CommandsWithNoReply,
    TestSyncCommandsNoReply,
    testing::ValuesIn(s_commandsWithNoReply),
    [](const testing::TestParamInfo<TestSyncCommandsNoReply::ParamType> &info) {
      return info.param.name;
    }
);

struct CommandReaderRow {
    std::string name;
    std::function<void(BteHci *hci, void *callback)> invoker;
    std::vector<uint8_t> expectedCommand;
    std::vector<uint8_t> eventData;
    std::vector<uint8_t> expectedReply;
};

class TestSyncCommandsReader:
    public testing::TestWithParam<CommandReaderRow>
{
};

TEST_P(TestSyncCommandsReader, testReaderCommands) {
    auto params = GetParam();

    MockBackend backend;

    BteClient *client = bte_client_new();
    BteHci *hci = bte_hci_get(client);

    void *expectedUserdata = (void*)0xdeadbeef;
    bte_client_set_userdata(client, expectedUserdata);
    using StatusCall = std::tuple<BteHci *, std::vector<uint8_t>, void*>;
    static std::vector<StatusCall> receivedReplies;
    static size_t replySize = params.expectedReply.size();
    receivedReplies.clear();
    struct Callbacks {
        static void replyCb(BteHci *hci, const uint8_t *reply, void *ud) {
            std::vector<uint8_t> replyData(replySize);
            memcpy(replyData.data(), reply, replySize);
            receivedReplies.push_back({hci, replyData, ud});
        }
    };

    params.invoker(hci, (void*)&Callbacks::replyCb);

    /* Verify that the expected command was sent */
    Buffer expectedCommand{params.expectedCommand};
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    backend.sendEvent(params.eventData);
    bte_handle_events();
    std::vector<StatusCall> expectedStatusCalls = {
        {hci, params.expectedReply, expectedUserdata}
    };
    ASSERT_EQ(receivedReplies, expectedStatusCalls);
    bte_client_unref(client);
}

static const std::vector<CommandReaderRow> s_commandsReader {
    {
        "read_local_name",
        [](BteHci *hci, void *cb) { bte_hci_read_local_name(hci, (BteHciReadLocalNameCb)cb); },
        {0x14, 0xc, 0},
        {HCI_COMMAND_COMPLETE, 4 + 6, 1, 0x14, 0xc, 0, 'A', ' ', 't', 'e', 's', 't'},
        {0, 'A', ' ', 't', 'e', 's', 't'},
    },
};
INSTANTIATE_TEST_CASE_P(
    CommandsReader,
    TestSyncCommandsReader,
    testing::ValuesIn(s_commandsReader),
    [](const testing::TestParamInfo<TestSyncCommandsReader::ParamType> &info) {
      return info.param.name;
    }
);

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
