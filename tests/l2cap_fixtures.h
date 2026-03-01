#ifndef BTE_TESTS_L2CAP_FIXTURES_H
#define BTE_TESTS_L2CAP_FIXTURES_H

#include "bte_l2cap_cpp.h"
#include "dummy_driver.h"
#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/l2cap_proto.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

class TestL2capFixture: public testing::Test
{
protected:
    using BufferList = Bte::BufferList;
    using L2cap = Bte::L2cap;

    TestL2capFixture() {
        bte_l2cap_reset();
    }

    void SetUp() override {
        m_backend.clear();
        m_cmdId = 1;
        m_localCid = 0x0040;
        m_remoteCid = 0x004f;
        dummy_driver_set_acl_limits(600, 20);
    }

    void TearDown() override {
        m_backend.onSendData({});
        while (!m_connections.empty()) {
            sendHciDisconnectionComplete(*m_connections.begin());
        }
        bte_handle_events();
    }

    static uint8_t low(uint16_t v) {
        return v & 0xff;
    }

    static uint8_t high(uint16_t v) {
        return v >> 8;
    }

    static uint8_t byte(int index, uint64_t v) {
        return (v >> (index * 8)) & 0xff;
    }

    Buffer makeL2capMessage(BteConnHandle connHandle,
                            BteL2capChannelId channelId,
                            const Buffer &data,
                            uint8_t flags = 0x20) {
        uint16_t size = data.size();
        return Buffer{
            low(connHandle), uint8_t(flags | high(connHandle)),
            low(size + 4), high(size + 4),
            low(size), high(size),
            low(channelId), high(channelId),
        } + data;
    }

    Buffer makeConnectRequest(uint8_t reqId, BteConnHandle connHandle,
                              BteL2capPsm psm) {
        return makeL2capMessage(connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_CONN_REQ,
            reqId,
            4, 0, /* cmd length */
            low(psm), high(psm),
            low(m_localCid), high(m_localCid), /* source CID */
        });
    }

    Buffer makeConnectResponse(
        uint8_t reqId,
        BteL2capChannelId destCid, BteL2capChannelId sourceCid,
        uint16_t result, uint16_t status) {
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_CONN_RSP,
            reqId,
            8, 0, /* command length */
            low(destCid), high(destCid),
            low(sourceCid), high(sourceCid),
            low(result), high(result),
            low(status), high(status),
        });
    }

    Buffer makeConnectResponse(
        uint8_t reqId, uint16_t result = BTE_L2CAP_CONN_RESP_RES_OK,
        uint16_t status = BTE_L2CAP_CONN_RESP_STATUS_NO_INFO) {
        return makeConnectResponse(reqId, m_remoteCid, m_localCid,
                                   result, status);
    }

    void sendConnectResponse(
        uint8_t reqId, uint16_t result = BTE_L2CAP_CONN_RESP_RES_OK,
        uint16_t status = BTE_L2CAP_CONN_RESP_STATUS_NO_INFO) {
        m_backend.sendData(makeConnectResponse(reqId, result, status));
    }

    L2cap connectTo(const BteBdAddr &address, BteL2capPsm psm,
                    BteConnHandle returnedHandle,
                    BteL2capChannelId remoteCid) {
        L2cap newL2cap;
        auto onConnected = [&newL2cap](std::optional<Bte::L2cap> l2cap,
                                  const BteL2capConnectionResponse &reply) {
            ASSERT_TRUE(l2cap.has_value());
            newL2cap = l2cap.value();
        };
        Bte::L2cap::newOutgoing(m_client, address, psm, {}, 0, onConnected);
        /* Send the statue reply for HCI create connection */
        uint8_t status = 0;
        m_backend.sendEvent({HCI_COMMAND_STATUS, 4, status, 1, 0x5, 0x4});
        bte_handle_events();
        /* Send the actual reply */
        const uint8_t eventSize = 1 + 2 + 6 + 1 + 1;
        uint8_t link_type = 1;
        uint8_t enc_mode = 0;
        m_backend.sendEvent(
            Buffer{HCI_CONNECTION_COMPLETE, eventSize, status,
                   low(returnedHandle), high(returnedHandle)} +
            address + Buffer{link_type, enc_mode});
        bte_handle_events();
        m_connections.insert(returnedHandle);

        m_backend.sendData(makeConnectResponse(m_cmdId++,
                                               remoteCid, m_localCid,
                                               BTE_L2CAP_CONN_RESP_RES_OK,
                                               BTE_L2CAP_CONN_RESP_STATUS_NO_INFO));
        bte_handle_events();
        m_backend.clear();
        return newL2cap;
    }

    Buffer makeConfigResponse(const Buffer &config, uint8_t reqId,
                              BteL2capChannelId sourceCid,
                              uint16_t result = L2CAP_CONFIG_RES_OK,
                              bool continuation = false) {
        uint8_t confSize = config.size();
        uint16_t flags = continuation ? L2CAP_CONFIG_FLAG_CONTINUATION : 0;
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_CONFIG_RSP,
            reqId,
            low(6 + confSize), high(6 + confSize), /* command length */
            low(sourceCid), high(sourceCid),
            low(flags), high(flags), /* flags */
            low(result), high(result), /* result */
            } + config);
    }

    void sendConfigResponse(const Buffer &config, uint8_t reqId,
                            uint16_t result = L2CAP_CONFIG_RES_OK,
                            bool continuation = false) {
        m_backend.sendData(makeConfigResponse(config, reqId, m_localCid,
                                              result, continuation));
    }

    Buffer makeConfigRequest(const Buffer &config, uint8_t reqId,
                             BteL2capChannelId destCid,
                             bool continuation = false) {
        uint8_t confSize = config.size();
        uint16_t flags = continuation ? L2CAP_CONFIG_FLAG_CONTINUATION : 0;
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_CONFIG_REQ,
            reqId,
            low(4 + confSize), high(4 + confSize), /* cmd length */
            low(destCid), high(destCid),
            low(flags), high(flags), /* flags (continuation) */
        } + config);
    }

    void sendConfigRequest(const Buffer &config, uint8_t reqId,
                           bool continuation = false) {
        m_backend.sendData(makeConfigRequest(config, reqId, m_localCid,
                                             continuation));
    }

    void runDefaultConfiguration(Bte::L2cap l2cap) {
        using L = Bte::L2cap;

        /* Send our request */
        l2cap.configure({}, [&](const L::ConfigureReply &r) {});
        ASSERT_EQ(l2cap.state(), BTE_L2CAP_WAIT_CONFIG_REQ_RSP);

        /* Receive a reply */
        sendConfigResponse(Buffer(), m_cmdId++);
        bte_handle_events();
        ASSERT_EQ(l2cap.state(), BTE_L2CAP_WAIT_CONFIG_REQ);

        /* Receive the peer request and reply to it */
        l2cap.onConfigureRequest([&](const L::ConfigureParams &params) {
            l2cap.setConfigureReply({});
        });
        sendConfigRequest({}, 42);
        bte_handle_events();
        ASSERT_EQ(l2cap.state(), BTE_L2CAP_OPEN);
        m_backend.clear();
    }

    BufferList makeData(const Buffer &data,
                        BteL2capChannelId destCid,
                        uint16_t mtu) {
        uint16_t totalSize = data.size();
        uint16_t writtenSize = 0;
        BufferList list;
        while (writtenSize < totalSize) {
            uint16_t sizeMax = writtenSize == 0 ? (mtu - 8) : (mtu - 4);
            uint16_t chunkLength = std::min(uint16_t(totalSize - writtenSize), sizeMax);
            Buffer packet;
            if (writtenSize == 0) {
                /* First packet */
                packet = {
                    0x00, 0x21, /* Connection handle */
                    low(4 + chunkLength), high(4 + chunkLength), /* Total length */
                    low(totalSize), high(totalSize), /* L2CAP length */
                    low(destCid), high(destCid),
                };
            } else {
                packet = {
                    0x00, 0x11, /* Connection handle */
                    low(chunkLength), high(chunkLength), /* Total length */
                };
            }
            list.push_back(packet + Buffer(data.begin() + writtenSize,
                                           data.begin() + writtenSize + chunkLength));
            writtenSize += chunkLength;
        }
        return list;
    }

    void sendData(const Buffer &data, uint16_t mtu = 348) {
        BufferList list = makeData(data, m_localCid, mtu);
        for (const Buffer &b: list) {
            m_backend.sendData(b);
        }
    }

    Buffer makeEchoSignal(uint8_t code, const Buffer &data, uint8_t reqId) {
        uint8_t dataSize = data.size();
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            code,
            reqId,
            low(dataSize), high(dataSize),
        } + data);
    }

    Buffer makeEchoRequest(const Buffer &data, uint8_t reqId) {
        return makeEchoSignal(L2CAP_SIGNAL_ECHO_REQ, data, reqId);
    }

    void sendEchoRequest(const Buffer &config, uint8_t reqId) {
        m_backend.sendData(makeEchoRequest(config, reqId));
    }

    Buffer makeEchoResponse(const Buffer &data, uint8_t reqId) {
        return makeEchoSignal(L2CAP_SIGNAL_ECHO_RSP, data, reqId);
    }

    void sendEchoResponse(const Buffer &config, uint8_t reqId) {
        m_backend.sendData(makeEchoResponse(config, reqId));
    }

    Buffer makeInfoRequest(BteL2capInfoType type, uint8_t reqId) {
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_INFO_REQ,
            reqId,
            2, 0, /* command length */
            low(type), high(type),
        });
    }

    void sendInfoRequest(BteL2capInfoType type, uint8_t reqId) {
        m_backend.sendData(makeInfoRequest(type, reqId));
    }

    Buffer makeInfoResponse(uint8_t reqId, BteL2capInfoType type,
                            uint16_t result, const Buffer &data = {}) {
        const uint16_t dataSize = data.size();
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_INFO_RSP,
            reqId,
            low(4 + dataSize), high(4 + dataSize),
            low(type), high(type),
            low(result), high(result),
        } + data);
    }

    Buffer makeInfoResponse(uint8_t reqId, BteL2capInfoType type,
                            uint16_t result, uint16_t mtu) {
        return makeInfoResponse(reqId, type, result, { low(mtu), high(mtu) });
    }

    Buffer makeInfoResponse(uint8_t reqId, BteL2capInfoType type,
                            uint16_t result, uint32_t features) {
        Buffer data {
            byte(0, features), byte(1, features),
            byte(2, features), byte(3, features),
        };
        return makeInfoResponse(reqId, type, result, data);
    }

    Buffer makeInfoResponse(uint8_t reqId, BteL2capInfoType type,
                            uint16_t result, uint64_t channels) {
        Buffer data {
            byte(0, channels), byte(1, channels),
            byte(2, channels), byte(3, channels),
            byte(4, channels), byte(5, channels),
            byte(6, channels), byte(7, channels),
        };
        return makeInfoResponse(reqId, type, result, data);
    }

    template <typename ...Params>
    void sendInfoResponse(uint8_t reqId, BteL2capInfoType type,
                          uint16_t result, Params&&... params) {
        m_backend.sendData(makeInfoResponse(reqId, type, result,
                                            std::forward<Params>(params)...));
    }

    Buffer makeHciCreateConnection(const BteBdAddr &address,
                                   BtePacketType packetType,
                                   uint8_t pageScanRepMode,
                                   uint16_t clockOffset,
                                   bool allowRoleSwitch) {
        uint8_t size = 13;
        const uint8_t *b = address.bytes;
        return {
            0x5, 0x4, size, b[0], b[1], b[2], b[3], b[4], b[5],
            low(packetType), high(packetType), pageScanRepMode,
            0, low(clockOffset), high(clockOffset), allowRoleSwitch
        };
    }

    void sendHciConnectionComplete(const BteBdAddr &address,
                                   BteConnHandle handle = 0x100,
                                   uint8_t status = 0) {
        const uint8_t eventSize = 1 + 2 + 6 + 1 + 1;
        uint8_t link_type = 1; /* ACL */
        uint8_t enc_mode = 0;
        m_backend.sendEvent(Buffer{
            HCI_CONNECTION_COMPLETE, eventSize, status,
            low(handle), high(handle)} +
            address + Buffer{link_type, enc_mode});
        m_connections.insert(handle);
    }

    Buffer makeHciAuthRequested(BteConnHandle handle) {
        return { 0x11, 0x4, 2, low(handle), high(handle) };
    }

    void sendHciAuthComplete(BteConnHandle handle, uint8_t status = 0) {
        const uint8_t eventSize = 1 + 2;
        m_backend.sendEvent({
            HCI_AUTH_COMPLETE, eventSize, status, low(handle), high(handle)
        });
    }

    Buffer makeHciCreateDisconnection(const BteBdAddr &address,
                                   BtePacketType packetType,
                                   uint8_t pageScanRepMode,
                                   uint16_t clockOffset,
                                   bool allowRoleSwitch) {
        uint8_t size = 13;
        const uint8_t *b = address.bytes;
        return {
            0x5, 0x4, size, b[0], b[1], b[2], b[3], b[4], b[5],
            low(packetType), high(packetType), pageScanRepMode,
            0, low(clockOffset), high(clockOffset), allowRoleSwitch
        };
    }

    void sendHciDisconnectionComplete(BteConnHandle handle,
                                      uint8_t reason = BTE_HCI_HOST_TIMEOUT)
    {
        const uint8_t eventSize = 1 + 2 + 1;
        uint8_t status = 0;
        m_backend.sendEvent({ HCI_DISCONNECTION_COMPLETE, eventSize, status,
                              low(handle), high(handle), reason });
        m_connections.erase(handle);
    }

    Buffer makeDisconnectResponse(uint8_t reqId,
                                  BteL2capChannelId destCid,
                                  BteL2capChannelId sourceCid) {
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_DISCONN_RSP,
            reqId,
            4, 0, /* command length */
            low(destCid), high(destCid),
            low(sourceCid), high(sourceCid),
        });
    }

    void sendDisconnectResponse(uint8_t reqId) {
        m_backend.sendData(makeDisconnectResponse(reqId,
                                                  m_remoteCid, m_localCid));
    }

    Buffer makeDisconnectRequest(uint8_t reqId,
                                 BteL2capChannelId destCid,
                                 BteL2capChannelId sourceCid) {
        return makeL2capMessage(m_connHandle, 0x1, Buffer{
            L2CAP_SIGNAL_DISCONN_REQ,
            reqId,
            4, 0, /* command length */
            low(destCid), high(destCid),
            low(sourceCid), high(sourceCid),
        });
    }

    void sendDisconnectRequest(uint8_t reqId) {
        m_backend.sendData(makeDisconnectRequest(reqId,
                                                 m_localCid, m_remoteCid));
    }

    MockBackend m_backend;
    Bte::Client m_client;
    BteL2capChannelId m_localCid;
    BteL2capChannelId m_remoteCid;
    BteConnHandle m_connHandle;
    uint8_t m_cmdId;
    std::set<BteConnHandle> m_connections;
};

class TestL2capFixtureConnected: public TestL2capFixture
{
protected:
    void SetUp() override {
        TestL2capFixture::SetUp();
        BteBdAddr address = {1, 2, 3, 4, 5, 6};
        m_connHandle = 0x0100;
        m_l2cap.reset(new L2cap(connectTo(address, BTE_L2CAP_PSM_SDP,
                                          m_connHandle, m_remoteCid)));
    }

protected:
    std::unique_ptr<Bte::L2cap> m_l2cap;
};

class TestL2capFixtureConfigured: public TestL2capFixtureConnected
{
protected:
    void SetUp() override {
        TestL2capFixtureConnected::SetUp();
        runDefaultConfiguration(*m_l2cap);
    }
};

#endif // BTE_TESTS_L2CAP_FIXTURES_H
