#ifndef BTE_MOCK_BACKEND_H
#define BTE_MOCK_BACKEND_H

#include "bt-embedded/backend.h"

#include <functional>
#include <vector>

class Buffer: public std::vector<uint8_t> {
public:
    using std::vector<uint8_t>::vector;
    Buffer(const std::vector<uint8_t> &data):
        std::vector<uint8_t>(data) {}
    Buffer(BteBuffer *buffer);

    BteBuffer *toBuffer(uint16_t max_packet_size = 0) const;
};

class MockBackend {
public:
    MockBackend();
    ~MockBackend();

    using InitCb = std::function<int()>;
    void onInit(const InitCb &initBb) {
        m_initCb = initBb;
    }

    using SendCb = std::function<int(BteBuffer*)>;
    void onSendCommand(const SendCb &sendCommandCb) {
        m_sendCommandCb = sendCommandCb;
    }
    void onSendData(const SendCb &sendDataCb) {
        m_sendDataCb = sendDataCb;
    }

    const std::vector<Buffer> sentCommands() const { return m_sentCommands; }
    Buffer lastCommand() const {
        return m_sentCommands.empty() ? Buffer() : m_sentCommands.back();
    }

    const std::vector<Buffer> sentData() const { return m_sentData; }
    Buffer lastData() const {
        return m_sentData.empty() ? Buffer() : m_sentData.back();
    }

    void sendEvent(const Buffer &buffer) {
        m_queuedEvents.push_back(buffer);
    }

    void sendData(const Buffer &buffer) {
        m_queuedEvents.push_back(buffer);
    }

    static MockBackend *instance() { return s_instance; }

    int callInit();
    int callSendCommand(BteBuffer *buffer);
    int callSendData(BteBuffer *buffer);
    int sendQueuedBuffers();

private:
    static MockBackend *s_instance;
    InitCb m_initCb;
    SendCb m_sendCommandCb;
    SendCb m_sendDataCb;
    std::vector<Buffer> m_sentCommands;
    std::vector<Buffer> m_sentData;
    std::vector<Buffer> m_queuedEvents;
    std::vector<Buffer> m_queuedData;
};

#endif /* BTE_MOCK_BACKEND_H */
