#include "helpers.h"
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
        "exit_periodic_inquiry",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_exit_periodic_inquiry(hci, cb);
        },
        {0x4, 0x4, 0}
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

TEST(Commands, Inquiry) {
    uint32_t requestedLap = 0xaabbcc;
    uint8_t requestedLen = 4;
    uint8_t requestedMaxResp = 9;
    uint8_t status = 0;

    AsyncCommandInvoker<StoredInquiryReply> invoker(
        [&](BteHci *hci, BteHciDoneCb statusCb, BteHciInquiryCb replyCb) {
            bte_hci_inquiry(hci, requestedLap, requestedLen, requestedMaxResp,
                            statusCb, replyCb);
        },
        {HCI_COMMAND_STATUS, 4, status, 1, 0x1, 0x4});

    /* Verify that the expected command was sent */
    Buffer expectedCommand {
        0x01, 0x04, 5, 0xcc, 0xbb, 0xaa, requestedLen, requestedMaxResp
    };
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    std::vector<BteHciReply> expectedStatusCalls = {{status}};
    ASSERT_EQ(invoker.receivedStatuses(), expectedStatusCalls);

    /* Emit the Inquiry Result events */
    MockBackend &backend = invoker.backend();
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
    ASSERT_EQ(invoker.replyCount(), 1);
    const StoredInquiryReply &reply = invoker.receivedReply();
    ASSERT_EQ(reply.num_responses, addresses.size());

    for (uint8_t i = 0; i < addresses.size(); i++) {
        ASSERT_EQ(reply.responses[i].address, addresses[i]);
    };
}

TEST(Commands, InquiryFailed) {
    uint32_t requestedLap = 0xaabbcc;
    uint8_t requestedLen = 4;
    uint8_t requestedMaxResp = 9;
    uint8_t status = HCI_HW_FAILURE;

    AsyncCommandInvoker<StoredInquiryReply> invoker(
        [&](BteHci *hci, BteHciDoneCb statusCb, BteHciInquiryCb replyCb) {
            bte_hci_inquiry(hci, requestedLap, requestedLen, requestedMaxResp,
                            statusCb, replyCb);
        },
        {HCI_COMMAND_STATUS, 4, status, 1, 0x1, 0x4});

    std::vector<BteHciReply> expectedStatusCalls = {{status}};
    ASSERT_EQ(invoker.receivedStatuses(), expectedStatusCalls);

    /* Verify that our callback has not been invoked */
    ASSERT_EQ(invoker.replyCount(), 0);
}

TEST(Commands, PeriodicInquiry) {
    uint16_t min_period = 0x1122, max_period = 0x3344;
    uint32_t requestedLap = 0xaabbcc;
    uint8_t requestedLen = 4;
    uint8_t requestedMaxResp = 9;
    uint8_t status = 0;

    AsyncCommandInvoker<StoredInquiryReply> invoker(
        [&](BteHci *hci, BteHciDoneCb statusCb, BteHciInquiryCb replyCb) {
            bte_hci_periodic_inquiry(
                hci, min_period, max_period,
                requestedLap, requestedLen, requestedMaxResp,
                statusCb, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x3, 0x4, status});

    /* Verify that the expected command was sent */
    Buffer expectedCommand {
        0x03, 0x04, 9, 0x44, 0x33, 0x22, 0x11, 0xcc, 0xbb, 0xaa, requestedLen, requestedMaxResp
    };
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    std::vector<BteHciReply> expectedStatusCalls = {{status}};
    ASSERT_EQ(invoker.receivedStatuses(), expectedStatusCalls);

    /* Emit the Inquiry Result events */
    MockBackend &backend = invoker.backend();
    std::vector<BteBdAddr> addresses;
    for (uint8_t i = 0; i < 5; i++) {
        addresses.emplace_back(BteBdAddr{ 1, 2, 3, 4, 5, uint8_t(6 + i)});
    };
    for (const BteBdAddr &address: addresses) {
        backend.sendEvent(createInquiryResult(address));
    }

    /* And an inquiry complete */
    backend.sendEvent({ HCI_INQUIRY_COMPLETE, 1, 0 });
    bte_handle_events();

    /* Verify that our callback has been invoked */
    ASSERT_EQ(invoker.replyCount(), 1);
    const StoredInquiryReply &reply = invoker.receivedReply();
    ASSERT_EQ(reply.num_responses, addresses.size());

    for (uint8_t i = 0; i < addresses.size(); i++) {
        ASSERT_EQ(reply.responses[i].address, addresses[i]);
    };

    /* Emit another round of results */
    addresses.clear();
    for (uint8_t i = 0; i < 3; i++) {
        addresses.emplace_back(BteBdAddr{ 1, 2, 3, 4, 5, uint8_t(6 + i)});
    };
    for (const BteBdAddr &address: addresses) {
        backend.sendEvent(createInquiryResult(address));
    }

    /* And an inquiry complete */
    backend.sendEvent({ HCI_INQUIRY_COMPLETE, 1, 0 });
    bte_handle_events();

    /* Verify that our callback has been invoked */
    ASSERT_EQ(invoker.replyCount(), 2);
    const StoredInquiryReply &reply2 = invoker.receivedReply();
    ASSERT_EQ(reply2.num_responses, addresses.size());

    for (uint8_t i = 0; i < addresses.size(); i++) {
        ASSERT_EQ(reply2.responses[i].address, addresses[i]);
    };
}

TEST(Commands, testReadLocalName) {
    GetterInvoker<BteHciReadLocalNameReply> invoker(
        [](BteHci *hci, BteHciReadLocalNameCb replyCb) {
            bte_hci_read_local_name(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4 + 6, 1, 0x14, 0xc, 0,
        'A', ' ', 't', 'e', 's', 't', '\0'});

    Buffer expectedCommand{0x14, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadLocalNameReply expectedReply = { 0, "A test" };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadLocalVersion) {
    GetterInvoker<BteHciReadLocalVersionReply> invoker(
        [](BteHci *hci, BteHciReadLocalVersionCb replyCb) {
            bte_hci_read_local_version(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4 + 8, 1, 0x1, 0x10, 0,
        1, 2, 3, 4, 5, 6, 7, 8});

    Buffer expectedCommand{0x1, 0x10, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadLocalVersionReply expectedReply = {
        0, 0x1, 0x302, 0x4, 0x605, 0x807 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadLocalFeatures) {
    GetterInvoker<BteHciReadLocalFeaturesReply> invoker(
        [](BteHci *hci, BteHciReadLocalFeaturesCb replyCb) {
            bte_hci_read_local_features(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4 + 8, 1, 0x3, 0x10, 0,
        1, 2, 3, 4, 5, 6, 7, 8});

    Buffer expectedCommand{0x3, 0x10, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadLocalFeaturesReply expectedReply = { 0, 0x807060504030201 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadBufferSize) {
    GetterInvoker<BteHciReadBufferSizeReply> invoker(
        [](BteHci *hci, BteHciReadBufferSizeCb replyCb) {
            bte_hci_read_buffer_size(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4 + 7, 1, 0x5, 0x10, 0, 1, 2, 3, 4, 5, 6, 7});

    Buffer expectedCommand{0x5, 0x10, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadBufferSizeReply expectedReply = { 0, 0x3, 0x201, 0x706, 0x504 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadBdAddr) {
    GetterInvoker<BteHciReadBdAddrReply> invoker(
        [](BteHci *hci, BteHciReadBdAddrCb replyCb) {
            bte_hci_read_bd_addr(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4 + 6, 1, 0x9, 0x10, 0, 1, 2, 3, 4, 5, 6});

    Buffer expectedCommand{0x9, 0x10, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadBdAddrReply expectedReply = { 0, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testVendorCommand) {
    MockBackend backend;

    Bte::Client client;
    auto &hci = client.hci();

    static std::vector<Buffer> doneCalls;

    uint16_t ocf = 0x123;
    std::vector<uint8_t> command { 1, 2, 3, 6, 5, 4};
    hci.vendorCommand(ocf, command, [&](const Buffer &buffer) {
        doneCalls.push_back(buffer);
    });

    /* Verify that the expected command was sent */
    Buffer expectedCommand{
        0x23, 0xfd, /* opcode */
        6, /* len */
        1, 2, 3, 6, 5, 4
    };
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    /* Send a status event */
    uint8_t status = 0;
    std::vector<uint8_t> expectedEvent {
        HCI_COMMAND_COMPLETE, 4,
        1, // packets
        0x23, 0xfd,
        status
    };
    backend.sendEvent(expectedEvent);
    bte_handle_events();
    std::vector<Buffer> expectedDoneCalls = {
        Buffer{expectedEvent},
    };
    ASSERT_EQ(doneCalls, expectedDoneCalls);
}

