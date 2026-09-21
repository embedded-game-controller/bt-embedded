#include "helpers.h"
#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"
#include "bt-embedded/internals.h"
#include <gtest/gtest.h>
#include <iostream>

TEST(Getters, BdAddress) {
    MockBackend backend;
    Bte::Client client;
    auto &hci = client.hci();

    BteBdAddr address;
    bool ok = hci.getBdAddr(&address);
    ASSERT_FALSE(ok);

    /* Read the BD address manually */
    hci.readBdAddr([&](const BteHciReadBdAddrReply &) {});
    BteBdAddr expectedAddress = {1, 2, 3, 4, 5, 6};
    Buffer reply = Buffer{
        HCI_COMMAND_COMPLETE, 4 + 6,
        1, // packets
        0x9, 0x10,
        0, // status
    } + expectedAddress;
    backend.sendEvent(reply);
    bte_handle_events();

    ok = hci.getBdAddr(&address);
    ASSERT_TRUE(ok);
    ASSERT_EQ(address, expectedAddress);
}
