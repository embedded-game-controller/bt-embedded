#include "helpers.h"
#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"
#include "bt-embedded/internals.h"
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
        "write_link_policy_settings",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_link_policy_settings(hci, 0x0123, 0x4567, cb); },
        {0xd, 0x8, 4, 0x23, 0x01, 0x67, 0x45}
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
        "set_event_filter_clear",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_event_filter(hci, BTE_HCI_EVENT_FILTER_TYPE_CLEAR,
                                     0, NULL, cb);
        },
        {0x5, 0xc, 1, 0}
    },
    {
        "set_event_filter_inquiry_all",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_event_filter(
                hci, BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT,
                BTE_HCI_COND_TYPE_INQUIRY_ALL, NULL, cb);
        },
        {0x5, 0xc, 2, 1, 0}
    },
    {
        "set_event_filter_inquiry_cod",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_event_filter(
                hci, BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT,
                BTE_HCI_COND_TYPE_INQUIRY_COD, "\0\1\2\xf1\xf2\xf3", cb);
        },
        {0x5, 0xc, 8, 1, 1, 0, 1, 2, 0xf1, 0xf2, 0xf3}
    },
    {
        "set_event_filter_inquiry_address",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_event_filter(
                hci, BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT,
                BTE_HCI_COND_TYPE_INQUIRY_ADDRESS, "\0\1\2\3\4\5", cb);
        },
        {0x5, 0xc, 8, 1, 2, 0, 1, 2, 3, 4, 5}
    },
    {
        "set_event_filter_conn_setup_all_switch_on",
        [](BteHci *hci, BteHciDoneCb cb) {
            uint8_t accept = BTE_HCI_COND_VALUE_CONN_SETUP_SWITCH_ON;
            bte_hci_set_event_filter(
                hci, BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP,
                BTE_HCI_COND_TYPE_CONN_SETUP_ALL, &accept, cb);
        },
        {0x5, 0xc, 3, 2, 0, 3}
    },
    {
        "set_event_filter_conn_setup_cod_auto_off",
        [](BteHci *hci, BteHciDoneCb cb) {
            uint8_t params[] = {
                0, 1, 2, 0xf1, 0xf2, 0xf3, /* COD and mask */
                BTE_HCI_COND_VALUE_CONN_SETUP_AUTO_OFF
            };
            bte_hci_set_event_filter(
                hci, BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP,
                BTE_HCI_COND_TYPE_CONN_SETUP_COD, params, cb);
        },
        {0x5, 0xc, 9, 2, 1, 0, 1, 2, 0xf1, 0xf2, 0xf3, 1}
    },
    {
        "set_event_filter_conn_setup_address_switch_off",
        [](BteHci *hci, BteHciDoneCb cb) {
            uint8_t params[] = {
                0, 1, 2, 3, 4, 5, /* BT address */
                BTE_HCI_COND_VALUE_CONN_SETUP_SWITCH_OFF
            };
            bte_hci_set_event_filter(
                hci, BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP,
                BTE_HCI_COND_TYPE_CONN_SETUP_ADDRESS, params, cb);
        },
        {0x5, 0xc, 9, 2, 2, 0, 1, 2, 3, 4, 5, 2}
    },
    {
        "write_pin_type",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_pin_type(hci, BTE_HCI_PIN_TYPE_FIXED, cb); },
        {0xa, 0xc, 1, BTE_HCI_PIN_TYPE_FIXED}
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
        "write_page_timeout",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_page_timeout(hci, 0xaabb, cb); },
        {0x18, 0xc, 2, 0xbb, 0xaa}
    },
    {
        "write_scan_enable",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_scan_enable(hci, BTE_HCI_SCAN_ENABLE_PAGE, cb); },
        {0x1a, 0xc, 1, BTE_HCI_SCAN_ENABLE_PAGE}
    },
    {
        "write_auth_enable",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_auth_enable(hci, BTE_HCI_AUTH_ENABLE_ON, cb); },
        {0x20, 0xc, 1, BTE_HCI_AUTH_ENABLE_ON}
    },
    {
        "write_class_of_device",
        [](BteHci *hci, BteHciDoneCb cb) {
            BteClassOfDevice cod{0x11, 0x22, 0x33};
            bte_hci_write_class_of_device(hci, &cod, cb); },
        {0x24, 0xc, 3, 0x11, 0x22, 0x33}
    },
    {
        "write_auto_flush_timeout",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_auto_flush_timeout(hci, 0x0123, 0x4567, cb); },
        {0x28, 0xc, 4, 0x23, 0x01, 0x67, 0x45}
    },
    {
        "set_ctrl_to_host_flow_control",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_ctrl_to_host_flow_control(
                hci, BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_SYNC, cb); },
        {0x31, 0xc, 1, BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_SYNC}
    },
    {
        "set_host_buffer_size",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_set_host_buffer_size(
                hci, 0x1234, 0x5678, 0x90, 0x4321, cb); },
        {0x33, 0xc, 7, 0x34, 0x12, 0x90, 0x78, 0x56, 0x21, 0x43}
    },
    {
        "write_link_sv_timeout",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_link_sv_timeout(hci, 0x0123, 0x4567, cb); },
        {0x37, 0xc, 4, 0x23, 0x01, 0x67, 0x45}
    },
    {
        "write_current_iac_lap",
        [](BteHci *hci, BteHciDoneCb cb) {
            BteLap laps[] = { 0x112233, 0x334455, 0x667788 };
            bte_hci_write_current_iac_lap(hci, 3, laps, cb); },
        {0x3a, 0xc, 10, 3,
            0x33, 0x22, 0x11, 0x55, 0x44, 0x33, 0x88, 0x77, 0x66}
    },
    {
        "write_inquiry_scan_type",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_inquiry_scan_type(
                hci, BTE_HCI_INQUIRY_SCAN_TYPE_STANDARD, cb); },
        {0x43, 0xc, 1, BTE_HCI_INQUIRY_SCAN_TYPE_STANDARD}
    },
    {
        "write_inquiry_mode",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_inquiry_mode(hci, BTE_HCI_INQUIRY_MODE_RSSI, cb); },
        {0x45, 0xc, 1, BTE_HCI_INQUIRY_MODE_RSSI}
    },
    {
        "write_page_scan_type",
        [](BteHci *hci, BteHciDoneCb cb) {
            bte_hci_write_page_scan_type(
                hci, BTE_HCI_PAGE_SCAN_TYPE_INTERLACED, cb); },
        {0x47, 0xc, 1, BTE_HCI_PAGE_SCAN_TYPE_INTERLACED}
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

TEST(Commands, testCreateConnection) {
    /* We test the C API here because the C++ one uses a fixed callback,
     * whereas we want to test that the API supports different callbacks */
    MockBackend backend;
    BteClient *client = bte_client_new();
    BteHci *hci = bte_hci_get(client);

    BteBdAddr address0 = {1, 2, 3, 4, 5, 6};
    BteBdAddr address1 = {8, 2, 3, 7, 5, 9};
    BtePacketType packet_type0 = BTE_PACKET_TYPE_DM1 | BTE_PACKET_TYPE_DM3;
    BtePacketType packet_type1 = BTE_PACKET_TYPE_DH1 | BTE_PACKET_TYPE_DH5;
    uint8_t page_scan_rep_mode0 = 2;
    uint8_t page_scan_rep_mode1 = 1;
    uint16_t clock_offset0 = BTE_HCI_CLOCK_OFFSET_INVALID;
    uint16_t clock_offset1 = 0x1122;
    bool allow_role_switch0 = true;
    bool allow_role_switch1 = false;

    using StoredReply = std::tuple<int,BteHciCreateConnectionReply>;
    using StoredStatusReply = std::tuple<int,uint8_t>;
    struct Callbacks {
        std::vector<StoredReply> replies;
        std::vector<StoredStatusReply> statusReplies;

        static void cb0(BteHci *, const BteHciCreateConnectionReply *reply,
                        void *userdata) {
            Callbacks *callbacks = static_cast<Callbacks*>(userdata);
            callbacks->replies.push_back({0, *reply});
        }
        static void cb1(BteHci *, const BteHciCreateConnectionReply *reply,
                        void *userdata) {
            Callbacks *callbacks = static_cast<Callbacks*>(userdata);
            callbacks->replies.push_back({1, *reply});
        }
        static void st0(BteHci *, const BteHciReply *reply, void *userdata) {
            Callbacks *callbacks = static_cast<Callbacks*>(userdata);
            callbacks->statusReplies.push_back({0, reply->status});
        }
        static void st1(BteHci *, const BteHciReply *reply, void *userdata) {
            Callbacks *callbacks = static_cast<Callbacks*>(userdata);
            callbacks->statusReplies.push_back({1, reply->status});
        }
    } callbacks;
    bte_client_set_userdata(client, &callbacks);

    /* Issue the first command */
    bte_hci_create_connection(hci, &address0, packet_type0,
                              page_scan_rep_mode0, clock_offset0,
                              allow_role_switch0,
                              &Callbacks::st0, &Callbacks::cb0);
    const uint8_t cmdSize = 6 + 2 + 1 + 1 + 2 + 1; /* one reserved byte */
    Buffer expectedCommand{0x5, 0x4, cmdSize};
    expectedCommand += address0;
    expectedCommand += Buffer{
        0x8, 0x4, page_scan_rep_mode0, 0x00, 0x00, 0x00, allow_role_switch0};
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    /* Send the status reply */
    uint8_t status = 0;
    backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();

    /* Issue a second command, becure the completion event for the first */
    bte_hci_create_connection(hci, &address1, packet_type1,
                              page_scan_rep_mode1, clock_offset1,
                              allow_role_switch1,
                              &Callbacks::st1, &Callbacks::cb1);
    expectedCommand = {0x5, 0x4, cmdSize};
    expectedCommand += address1;
    expectedCommand += Buffer{
        0x10, 0x40, page_scan_rep_mode1, 0x00, 0x22, 0x11, allow_role_switch1};
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    /* Send the status reply for this second command */
    backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    bte_handle_events();

    /* Now send the replies, but in inverted order */
    const uint8_t eventSize = 1 + 2 + 6 + 1 + 1;
    uint8_t link_type0 = 1;
    uint8_t link_type1 = 0;
    uint8_t enc_mode0 = 0;
    uint8_t enc_mode1 = 1;
    backend.sendEvent(
        Buffer{HCI_CONNECTION_COMPLETE, eventSize, status, 0x33, 0x44} +
        address1 + Buffer{link_type1, enc_mode1});
    backend.sendEvent(
        Buffer{HCI_CONNECTION_COMPLETE, eventSize, status, 0x55, 0x66} +
        address0 + Buffer{link_type0, enc_mode0});
    bte_handle_events();

    std::vector<StoredStatusReply> expectedStatusReplies = {
        { 0, 0 },
        { 1, 0 },
    };
    ASSERT_EQ(callbacks.statusReplies, expectedStatusReplies);

    std::vector<StoredReply> expectedReplies = {
        { 1, {status,link_type1, 0x4433, address1, enc_mode1} },
        { 0, {status,link_type0, 0x6655, address0, enc_mode0} },
    };
    ASSERT_EQ(callbacks.replies, expectedReplies);

    ASSERT_EQ(_bte_hci_dev.num_pending_commands, 0);

    /* Now repeat the test, making the first request fail; we should still
     * receive the second, and have no leaks */
    callbacks.replies.clear();
    callbacks.statusReplies.clear();
    uint8_t status0 = HCI_NO_CONNECTION;
    bte_hci_create_connection(hci, &address0, packet_type0,
                              page_scan_rep_mode0, clock_offset0,
                              allow_role_switch0,
                              &Callbacks::st0, &Callbacks::cb0);
    backend.sendEvent({HCI_COMMAND_STATUS, 4, status0, 1, 0x5, 0x4});
    bte_handle_events();
    bte_hci_create_connection(hci, &address1, packet_type1,
                              page_scan_rep_mode1, clock_offset1,
                              allow_role_switch1,
                              &Callbacks::st1, &Callbacks::cb1);
    backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
    backend.sendEvent(
        Buffer{HCI_CONNECTION_COMPLETE, eventSize, status, 0x77, 0x88} +
        address1 + Buffer{link_type1, enc_mode1});
    bte_handle_events();

    expectedStatusReplies = {
        { 0, HCI_NO_CONNECTION },
        { 1, 0 },
    };
    ASSERT_EQ(callbacks.statusReplies, expectedStatusReplies);

    expectedReplies = {
        { 1, {status,link_type1, 0x8877, address1, enc_mode1} },
    };
    ASSERT_EQ(callbacks.replies, expectedReplies);

    ASSERT_EQ(_bte_hci_dev.num_pending_commands, 0);

    bte_client_unref(client);
}

TEST(Commands, testLinkKeyReqReply) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    BteLinkKey key = {4, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<BteHciLinkKeyReqReply> replies;
    hci.linkKeyReqReply(address, key, [&](const BteHciLinkKeyReqReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0xb, 0x4, 22};
    expectedCommand += address;
    expectedCommand += key;
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    uint8_t status = 0;
    backend.sendEvent(Buffer{HCI_COMMAND_COMPLETE, 10, 1, 0xb, 0x4, status} +
                      address);
    bte_handle_events();

    std::vector<BteHciLinkKeyReqReply> expectedReplies = {
        { status, address },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testLinkKeyReqNegReply) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteHciLinkKeyReqReply> replies;
    hci.linkKeyReqNegReply(address, [&](const BteHciLinkKeyReqReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0xc, 0x4, 6};
    expectedCommand += address;
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    uint8_t status = 0;
    backend.sendEvent(Buffer{HCI_COMMAND_COMPLETE, 10, 1, 0xc, 0x4, status} +
                      address);
    bte_handle_events();

    std::vector<BteHciLinkKeyReqReply> expectedReplies = {
        { status, address },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testPinCodeReqReply) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<uint8_t> pin = {'A', ' ', 'p', 'i', 'n'};
    std::vector<BteHciPinCodeReqReply> replies;
    hci.pinCodeReqReply(address, pin, [&](const BteHciPinCodeReqReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0xd, 0x4, 23};
    expectedCommand += address;
    expectedCommand += uint8_t(pin.size());
    expectedCommand += pin;
    Buffer lastCommand = backend.lastCommand();
    Buffer commandStart(lastCommand.begin(),
                        lastCommand.begin() + expectedCommand.size());
    ASSERT_EQ(commandStart, expectedCommand);
    /* The remaining bytes can be garbage, but they must be present */
    ASSERT_EQ(lastCommand.size(), 3 + 23);

    uint8_t status = 0;
    backend.sendEvent(Buffer{HCI_COMMAND_COMPLETE, 10, 1, 0xd, 0x4, status} +
                      address);
    bte_handle_events();

    std::vector<BteHciPinCodeReqReply> expectedReplies = {
        { status, address },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testPinCodeReqNegReply) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteHciPinCodeReqReply> replies;
    hci.pinCodeReqNegReply(address, [&](const BteHciPinCodeReqReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0xe, 0x4, 6};
    expectedCommand += address;
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    uint8_t status = 0;
    backend.sendEvent(Buffer{HCI_COMMAND_COMPLETE, 10, 1, 0xe, 0x4, status} +
                      address);
    bte_handle_events();

    std::vector<BteHciPinCodeReqReply> expectedReplies = {
        { status, address },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testReadLinkPolicySettings) {
    BteHciConnHandle conn = 0x0123;
    GetterInvoker<BteHciReadLinkPolicySettingsReply> invoker(
        [&](BteHci *hci, BteHciReadLinkPolicySettingsCb replyCb) {
            bte_hci_read_link_policy_settings(hci, conn, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 5, 1, 0xc, 0x8, 0, 0x23, 0x01, 0x78, 0x56 });

    Buffer expectedCommand{0xc, 0x8, 2, 0x23, 0x01};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadLinkPolicySettingsReply expectedReply = { 0, conn, 0x5678 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadPinType) {
    GetterInvoker<BteHciReadPinTypeReply> invoker(
        [](BteHci *hci, BteHciReadPinTypeCb replyCb) {
            bte_hci_read_pin_type(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x9, 0xc, 0, 1 });

    Buffer expectedCommand{0x9, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadPinTypeReply expectedReply = { 0, BTE_HCI_PIN_TYPE_FIXED };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadStoredLinkKeyByAddress) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    using LinkKeyReply = Bte::Client::Hci::ReadStoredLinkKeyReply;
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<StoredTypes::ReadStoredLinkKeyReply> replies;
    hci.readStoredLinkKey(address, [&](const LinkKeyReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0xd, 0xc, 7};
    expectedCommand += address;
    expectedCommand += uint8_t(0);
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    BteLinkKey key = {4, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16};
    Buffer returnLinkKeyEvent { HCI_RETURN_LINK_KEYS, 1 + 6 + 16, 1 };
    returnLinkKeyEvent += address;
    returnLinkKeyEvent += key;
    backend.sendEvent(returnLinkKeyEvent);
    uint8_t status = 0;
    backend.sendEvent({
        HCI_COMMAND_COMPLETE, 10, 1, 0xd, 0xc, status, 0x34, 0x12, 0x01, 0x00,
    });
    bte_handle_events();

    BteHciStoredLinkKey storedKey = { address, key };
    std::vector<StoredTypes::ReadStoredLinkKeyReply> expectedReplies = {
        { status, 0x1234, {storedKey}},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testReadStoredLinkKeyAll) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    using LinkKeyReply = Bte::Client::Hci::ReadStoredLinkKeyReply;
    std::vector<StoredTypes::ReadStoredLinkKeyReply> replies;
    hci.readStoredLinkKey([&](const LinkKeyReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0xd, 0xc, 7};
    Buffer lastCommand = backend.lastCommand();
    Buffer commandStart(lastCommand.begin(), lastCommand.begin() + 3);
    ASSERT_EQ(commandStart, expectedCommand);
    ASSERT_EQ(lastCommand[3 + 6], uint8_t(1));

    auto createEvent = [](const std::vector<BteHciStoredLinkKey> &eventData) {
        size_t count = eventData.size();
        Buffer event {
            HCI_RETURN_LINK_KEYS,
            uint8_t(1 + count * (6 + 16)), uint8_t(count)
        };
        for (const auto &e: eventData) event += e.address;
        for (const auto &e: eventData) event += e.key;
        return event;
    };

    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    BteLinkKey key = {4, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16};
    std::vector<BteHciStoredLinkKey> eventData;

    /* first event: 3 elements */
    for (int i = 0; i < 3; i++) {
        BteBdAddr a(address);
        BteLinkKey k(key);
        a.bytes[0] = k.bytes[0] = i * 4;
        eventData.emplace_back(BteHciStoredLinkKey{ a, k });
    };
    backend.sendEvent(createEvent(eventData));

    /* second event: 2 elements */
    eventData.clear();
    for (int i = 0; i < 2; i++) {
        BteBdAddr a(address);
        BteLinkKey k(key);
        a.bytes[1] = k.bytes[1] = i * 5;
        eventData.emplace_back(BteHciStoredLinkKey{ a, k });
    };
    backend.sendEvent(createEvent(eventData));

    uint8_t status = 0;
    backend.sendEvent({
        HCI_COMMAND_COMPLETE, 10, 1, 0xd, 0xc, status, 0x34, 0x12, 3 + 2, 0x00,
    });
    bte_handle_events();

    std::vector<StoredTypes::ReadStoredLinkKeyReply> expectedReplies = {
        { status, 0x1234, {
            {{0, 2, 3, 4, 5, 6},
                {0, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
            {{4, 2, 3, 4, 5, 6},
                {4, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
            {{8, 2, 3, 4, 5, 6},
                {8, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
            {{1, 0, 3, 4, 5, 6},
                {4, 0, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
            {{1, 5, 3, 4, 5, 6},
                {4, 5, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
        }},
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testWriteStoredLinkKey) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    const std::vector<BteHciStoredLinkKey> keys {
        {{1, 2, 3, 4, 5, 6},
            {0, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
        {{10, 11, 12, 13, 14, 15},
            {4, 3, 2, 1, 8, 7, 6, 5, 9, 10, 11, 12, 13, 14, 15, 16}},
    };
    std::vector<BteHciWriteStoredLinkKeyReply> replies;
    hci.writeStoredLinkKey(keys, [&](const BteHciWriteStoredLinkKeyReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0x11, 0xc, (6 + 16) * 2};
    expectedCommand += keys[0].address;
    expectedCommand += keys[1].address;
    expectedCommand += keys[0].key;
    expectedCommand += keys[1].key;
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    uint8_t status = 0;
    /* Pretend that only one key has been written */
    backend.sendEvent({ HCI_COMMAND_COMPLETE, 5, 1, 0x11, 0xc, status, 1 });
    bte_handle_events();

    std::vector<BteHciWriteStoredLinkKeyReply> expectedReplies = {
        { status, 1 },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testDeleteStoredLinkKeyByAddress) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    std::vector<BteHciDeleteStoredLinkKeyReply> replies;
    hci.deleteStoredLinkKey(address,
                            [&](const BteHciDeleteStoredLinkKeyReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0x12, 0xc, 7};
    expectedCommand += address;
    expectedCommand += uint8_t(0);
    ASSERT_EQ(backend.lastCommand(), expectedCommand);

    uint8_t status = 0;
    backend.sendEvent({ HCI_COMMAND_COMPLETE, 5, 1, 0x12, 0xc, status, 1, 0 });
    bte_handle_events();

    std::vector<BteHciDeleteStoredLinkKeyReply> expectedReplies = {
        { status, 1 },
    };
    ASSERT_EQ(replies, expectedReplies);
}

TEST(Commands, testDeleteStoredLinkKeyAll) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    std::vector<BteHciDeleteStoredLinkKeyReply> replies;
    hci.deleteStoredLinkKey([&](const BteHciDeleteStoredLinkKeyReply &r) {
        replies.push_back(r);
    });

    Buffer expectedCommand{0x12, 0xc, 7};
    Buffer lastCommand = backend.lastCommand();
    Buffer commandStart(lastCommand.begin(), lastCommand.begin() + 3);
    ASSERT_EQ(commandStart, expectedCommand);
    ASSERT_EQ(lastCommand[3 + 6], uint8_t(1));

    uint8_t status = 0;
    backend.sendEvent({ HCI_COMMAND_COMPLETE, 5, 1, 0x12, 0xc, status, 4, 0 });
    bte_handle_events();

    std::vector<BteHciDeleteStoredLinkKeyReply> expectedReplies = {
        { status, 4 },
    };
    ASSERT_EQ(replies, expectedReplies);
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

TEST(Commands, testReadPageTimeout) {
    GetterInvoker<BteHciReadPageTimeoutReply> invoker(
        [](BteHci *hci, BteHciReadPageTimeoutCb replyCb) {
            bte_hci_read_page_timeout(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 6, 1, 0x17, 0xc, 0, 0xaa, 0xbb });

    Buffer expectedCommand{0x17, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadPageTimeoutReply expectedReply = { 0, 0xbbaa };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadScanEnable) {
    GetterInvoker<BteHciReadScanEnableReply> invoker(
        [](BteHci *hci, BteHciReadScanEnableCb replyCb) {
            bte_hci_read_scan_enable(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x19, 0xc, 0, 3 });

    Buffer expectedCommand{0x19, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadScanEnableReply expectedReply = {
        0, BTE_HCI_SCAN_ENABLE_INQ_PAGE
    };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadAuthEnable) {
    GetterInvoker<BteHciReadAuthEnableReply> invoker(
        [](BteHci *hci, BteHciReadAuthEnableCb replyCb) {
            bte_hci_read_auth_enable(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x1f, 0xc, 0, 1 });

    Buffer expectedCommand{0x1f, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadAuthEnableReply expectedReply = { 0, BTE_HCI_AUTH_ENABLE_ON };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadClassOfDevice) {
    GetterInvoker<BteHciReadClassOfDeviceReply> invoker(
        [](BteHci *hci, BteHciReadClassOfDeviceCb replyCb) {
            bte_hci_read_class_of_device(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 7, 1, 0x23, 0xc, 0, 0xaa, 0xbb, 0xcc });

    Buffer expectedCommand{0x23, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadClassOfDeviceReply expectedReply = { 0, {0xaa, 0xbb, 0xcc}};
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadAutoFlushTimeout) {
    BteHciConnHandle conn = 0x0123;
    GetterInvoker<BteHciReadAutoFlushTimeoutReply> invoker(
        [&](BteHci *hci, BteHciReadAutoFlushTimeoutCb replyCb) {
            bte_hci_read_auto_flush_timeout(hci, conn, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 5, 1, 0x27, 0xc, 0, 0x23, 0x01, 0x78, 0x56 });

    Buffer expectedCommand{0x27, 0xc, 2, 0x23, 0x01};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadAutoFlushTimeoutReply expectedReply = { 0, conn, 0x5678 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadLinkSvTimeout) {
    BteHciConnHandle conn = 0x0123;
    GetterInvoker<BteHciReadLinkSvTimeoutReply> invoker(
        [&](BteHci *hci, BteHciReadLinkSvTimeoutCb replyCb) {
            bte_hci_read_link_sv_timeout(hci, conn, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 5, 1, 0x36, 0xc, 0, 0x23, 0x01, 0x78, 0x56 });

    Buffer expectedCommand{0x36, 0xc, 2, 0x23, 0x01};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadLinkSvTimeoutReply expectedReply = { 0, conn, 0x5678 };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadCurrentIacLap) {
    GetterInvoker<StoredTypes::ReadCurrentIacLapReply,
                  BteHciReadCurrentIacLapReply> invoker(
        [](BteHci *hci, BteHciReadCurrentIacLapCb replyCb) {
            bte_hci_read_current_iac_lap(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 5 + 3 * 2, 1, 0x39, 0xc, 0, 2,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66});

    Buffer expectedCommand{0x39, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    StoredTypes::ReadCurrentIacLapReply expectedReply = {
        0, { 0x332211, 0x665544}
    };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadInquiryScanType) {
    GetterInvoker<BteHciReadInquiryScanTypeReply> invoker(
        [](BteHci *hci, BteHciReadInquiryScanTypeCb replyCb) {
            bte_hci_read_inquiry_scan_type(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x42, 0xc, 0, 1 });

    Buffer expectedCommand{0x42, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadInquiryScanTypeReply expectedReply = {
        0, BTE_HCI_INQUIRY_SCAN_TYPE_INTERLACED };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadInquiryMode) {
    GetterInvoker<BteHciReadInquiryModeReply> invoker(
        [](BteHci *hci, BteHciReadInquiryModeCb replyCb) {
            bte_hci_read_inquiry_mode(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x44, 0xc, 0, 0 });

    Buffer expectedCommand{0x44, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadInquiryModeReply expectedReply = {
        0, BTE_HCI_INQUIRY_MODE_STANDARD };
    ASSERT_EQ(invoker.receivedReply(), expectedReply);
}

TEST(Commands, testReadPageScanType) {
    GetterInvoker<BteHciReadPageScanTypeReply> invoker(
        [](BteHci *hci, BteHciReadPageScanTypeCb replyCb) {
            bte_hci_read_page_scan_type(hci, replyCb);
        },
        {HCI_COMMAND_COMPLETE, 4, 1, 0x46, 0xc, 0, 0 });

    Buffer expectedCommand{0x46, 0xc, 0};
    ASSERT_EQ(invoker.sentCommand(), expectedCommand);

    BteHciReadPageScanTypeReply expectedReply = {
        0, BTE_HCI_PAGE_SCAN_TYPE_STANDARD };
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

