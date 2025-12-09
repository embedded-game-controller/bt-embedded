#include "mock_backend.h"

#include "bt-embedded/backend.h"
#include "bt-embedded/internals.h"

#include <cassert>
#include <errno.h>

MockBackend *MockBackend::s_instance = nullptr;

Buffer::Buffer(BteBuffer *buffer)
{
    resize(buffer->size);

    BteBufferReader reader;
    bte_buffer_reader_init(&reader, buffer);
    uint16_t read = bte_buffer_reader_read(&reader, data(), size());
    assert(read == size());
}

static void buffer_free(BteBuffer *buffer)
{
    if (buffer->next) buffer_free(buffer->next);
    delete [] reinterpret_cast<uint8_t*>(buffer);
}

BteBuffer *Buffer::toBuffer(uint16_t max_packet_size) const
{
    if (max_packet_size == 0) max_packet_size = size();

    uint16_t allocated = 0;
    BteBuffer *buffer = nullptr, *prev = nullptr;
    while (allocated < size()) {
        uint16_t packet_size = max_packet_size;
        if (allocated + packet_size > size()) {
            packet_size = size() - allocated;
        }
        BteBuffer *b = (BteBuffer*)new uint8_t[sizeof(BteBuffer) + packet_size];
        b->ref_count = 1;
        b->free_func = buffer_free;
        b->total_size = size();
        b->size = packet_size;
        b->next = nullptr;
        memcpy(b->data, data() + allocated, packet_size);
        if (prev) prev->next = b;
        else buffer = b;
        prev = b;
        allocated += packet_size;
    }

    BteBufferWriter writer;
    bte_buffer_writer_init(&writer, buffer);
    bte_buffer_writer_write(&writer, data(), size());
    return buffer;
}

MockBackend::MockBackend()
{
    s_instance = this;
}

MockBackend::~MockBackend()
{
    s_instance = nullptr;
}

int MockBackend::callInit()
{
    return m_initCb ? m_initCb() : 0;
}

int MockBackend::callSendCommand(BteBuffer *buffer)
{
    m_sentCommands.emplace_back(buffer);
    if (m_sendCommandCb) {
        return m_sendCommandCb(buffer);
    }
    return 0;
}

int MockBackend::callSendData(BteBuffer *buffer)
{
    m_sentData.emplace_back(buffer);
    if (m_sendDataCb) {
        return m_sendDataCb(buffer);
    }
    return 0;
}

int MockBackend::sendQueuedBuffers()
{
    int count = m_queuedEvents.size() + m_queuedData.size();
    for (const Buffer &b: m_queuedEvents) {
        BteBuffer *buffer = b.toBuffer();
        _bte_hci_dev_handle_event(buffer);
        bte_buffer_unref(buffer);
    }
    for (const Buffer &b: m_queuedData) {
        BteBuffer *buffer = b.toBuffer();
        _bte_hci_dev_handle_data(buffer);
        bte_buffer_unref(buffer);
    }
    m_queuedEvents.clear();
    m_queuedData.clear();
    return count;
}

static int mock_init()
{
    MockBackend *backend = MockBackend::instance();
    return backend->callInit();
}

static int mock_handle_events(bool wait_for_events, uint32_t timeout_ms)
{
    MockBackend *backend = MockBackend::instance();
    return backend->sendQueuedBuffers();
}

static int mock_hci_send_command(BteBuffer *buffer)
{
    MockBackend *backend = MockBackend::instance();
    return backend->callSendCommand(buffer);
}

static int mock_hci_send_data(BteBuffer *buffer)
{
    MockBackend *backend = MockBackend::instance();
    return backend->callSendData(buffer);
}

static int mock_deinit()
{
    return 0;
}

const BteBackend _bte_backend = {
    .init = mock_init,

    .handle_events = mock_handle_events,

    .hci_send_command = mock_hci_send_command,
    .hci_send_data = mock_hci_send_data,

    .deinit = mock_deinit,
};
