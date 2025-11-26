#include "bte_cpp.h"
#include "mock_backend.h"
#include "type_utils.h"

#include <gtest/gtest.h>

TEST(Events, testConnectionRequested)
{
    MockBackend backend;
    Bte::Client client0, client1, client2;
    auto &hci0 = client0.hci();
    auto &hci1 = client1.hci();
    auto &hci2 = client2.hci();

    /* The first int is the index of the hci instance */
    using Call = std::tuple<int, BteBdAddr, BteClassOfDevice, uint8_t>;
    std::vector<Call> calls;
    hci0.onConnectionRequest([&](const BteBdAddr &address,
                                 const BteClassOfDevice &cod,
                                 uint8_t link_type) {
        calls.push_back({0, address, cod, link_type});
        return false;
    });
    hci1.onConnectionRequest([&](const BteBdAddr &address,
                                 const BteClassOfDevice &cod,
                                 uint8_t link_type) {
        calls.push_back({1, address, cod, link_type});
        return true;
    });
    hci2.onConnectionRequest([&](const BteBdAddr &address,
                                 const BteClassOfDevice &cod,
                                 uint8_t link_type) {
        calls.push_back({2, address, cod, link_type});
        return false;
    });

    /* Emit the ConnectionRequest event */
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    BteClassOfDevice cod = {7, 8, 9};
    uint8_t link_type = 2;
    backend.sendEvent(
        Buffer{ HCI_CONNECTION_REQUEST, 6 + 3 + 1 } + address + cod +
        Buffer{link_type});
    bte_handle_events();

    /* The second handler returned true, so the third should not be invoked */
    std::vector<Call> expectedCalls = {
        {0, address, cod, link_type},
        {1, address, cod, link_type},
    };
    ASSERT_EQ(calls, expectedCalls);
}

TEST(Events, testLinkKeyRequested)
{
    MockBackend backend;
    Bte::Client client0, client1, client2;
    auto &hci0 = client0.hci();
    auto &hci1 = client1.hci();
    auto &hci2 = client2.hci();

    /* The first int is the index of the hci instance */
    using Call = std::tuple<int, BteBdAddr>;
    std::vector<Call> calls;
    hci0.onLinkKeyRequest([&](const BteBdAddr &address) {
        calls.push_back({0, address});
        return false;
    });
    hci1.onLinkKeyRequest([&](const BteBdAddr &address) {
        calls.push_back({1, address});
        return true;
    });
    hci2.onLinkKeyRequest([&](const BteBdAddr &address) {
        calls.push_back({2, address});
        return true;
    });

    /* Emit the LinkKeyRequest event */
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    uint8_t *b = address.bytes;
    backend.sendEvent({
        HCI_LINK_KEY_REQUEST, 6, b[0], b[1], b[2], b[3], b[4], b[5]
    });
    bte_handle_events();

    /* The second handler returned true, so the third should not be invoked */
    std::vector<Call> expectedCalls = {
        {0, address},
        {1, address},
    };
    ASSERT_EQ(calls, expectedCalls);
}

TEST(Events, testPinCodeRequested)
{
    MockBackend backend;
    Bte::Client client0, client1, client2;
    auto &hci0 = client0.hci();
    auto &hci1 = client1.hci();
    auto &hci2 = client2.hci();

    /* The first int is the index of the hci instance */
    using Call = std::tuple<int, BteBdAddr>;
    std::vector<Call> calls;
    hci0.onPinCodeRequest([&](const BteBdAddr &address) {
        calls.push_back({0, address});
        return false;
    });
    hci1.onPinCodeRequest([&](const BteBdAddr &address) {
        calls.push_back({1, address});
        return true;
    });
    hci2.onPinCodeRequest([&](const BteBdAddr &address) {
        calls.push_back({2, address});
        return true;
    });

    /* Emit the PinCodeRequest event */
    BteBdAddr address = {1, 2, 3, 4, 5, 6};
    uint8_t *b = address.bytes;
    backend.sendEvent({
        HCI_PIN_CODE_REQUEST, 6, b[0], b[1], b[2], b[3], b[4], b[5]
    });
    bte_handle_events();

    /* The second handler returned true, so the third should not be invoked */
    std::vector<Call> expectedCalls = {
        {0, address},
        {1, address},
    };
    ASSERT_EQ(calls, expectedCalls);
}
