#ifndef BTE_BUFFER_H
#define BTE_BUFFER_H

#include "platform_defs.h"
#include "types.h"

#include <malloc.h>
#ifdef __cplusplus
#include <atomic>
using namespace std;
#else
#include <stdatomic.h>
#endif
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup bufferG Working with data buffers
 *
 * @brief Data buffer handling
 *
 * Functions for creating, reading and writing data packets.
 */

/**
 * @addtogroup bufferG
 * @{
 */

/** @cond */
struct bte_buffer_t {
    /* The first members are only used in the head of the linked list, but it's
     * not a big waste of space, since (at least on the Wii) some bytes would
     * anyway be lost because of the alignment requirements. */
    atomic_int ref_count;
    void (*free_func)(BteBuffer *);
    uint16_t total_size;

    uint16_t size;
    struct bte_buffer_t *next;
    uint8_t data[0] BTE_BUFFER_ALIGN;
} BTE_BUFFER_ALIGN;
/** @endcond */

static inline void *bte_buffer_malloc(uint16_t size)
{
#ifdef BTE_BUFFER_ALIGNMENT_SIZE
    return bte_malloc_aligned(BTE_BUFFER_ALIGNMENT_SIZE,
                              (size + BTE_BUFFER_ALIGNMENT_SIZE - 1) &
                              ~(BTE_BUFFER_ALIGNMENT_SIZE - 1));
#else
    return bte_malloc(size);
#endif
}

static void bte_buffer_unref(BteBuffer *buffer);

/* Used for small packets (TODO: clarify) */
static inline BteBuffer *bte_buffer_alloc_contiguous(uint16_t size)
{
    BteBuffer *b = (BteBuffer*)bte_buffer_malloc(sizeof(BteBuffer) + size);
    b->ref_count = 1;
    b->free_func = (void (*)(BteBuffer *))bte_free;
    b->total_size = b->size = size;
    b->next = NULL;
    return b;
}

/**
 * @brief Allocate a data buffer
 *
 * Create a data buffer of the given size, as a linked list of buffers where
 * each buffer contains at most \a block_size bytes.
 *
 * @note One shouldn't usually need to use this function: buffers to be used
 * with a certain layer of the API should be allocated using the functions
 * provided by that API, such as bte_l2cap_create_message().
 *
 * @param size The total size of the data
 * @param block_size Maximum size for each element of the linked list
 *
 * @return A new linked list of buffers
 */
static inline BteBuffer *bte_buffer_alloc(uint16_t size, uint16_t block_size)
{
    uint16_t remaining = size;
    BteBuffer *head = NULL, *tail = NULL;
    while (remaining > 0) {
        uint16_t buffer_size = remaining > block_size ? block_size : remaining;
        BteBuffer *buffer = bte_buffer_alloc_contiguous(buffer_size);
        if (!buffer) {
            bte_buffer_unref(head);
            return NULL;
        }
        buffer->total_size = size;
        if (!tail) {
            head = tail = buffer;
        } else {
            tail->next = buffer;
            tail = buffer;
        }
        remaining -= buffer_size;
    }
    return head;
}

static inline void bte_buffer_shrink(BteBuffer *buffer, uint16_t size)
{
    buffer->total_size = size;
    while (buffer) {
        if (buffer->size > size) buffer->size = size;
        size -= buffer->size;
        buffer = buffer->next;
    }
}

/**
 * @brief Increment the usage count of the buffer
 *
 * Add a reference to \a buffer, to ensure that it's not destroyed while the
 * reference is being held.
 *
 * @param buffer The buffer
 *
 * @return The same buffer
 */
static inline BteBuffer *bte_buffer_ref(BteBuffer *buffer)
{
    atomic_fetch_add(&buffer->ref_count, 1);
    return buffer;
}

/**
 * @brief Decrement the usage count of the buffer
 *
 * Remove a reference to \a buffer. The buffer will be destroyed when all
 * references are removed.
 *
 * @param buffer The buffer
 */
static inline void bte_buffer_unref(BteBuffer *buffer)
{
    if (atomic_fetch_sub(&buffer->ref_count, 1) == 1) {
        if (buffer->next) bte_buffer_unref(buffer->next);
        if (buffer->free_func) buffer->free_func(buffer);
    }
}

static inline uint8_t *bte_buffer_contiguous_data(BteBuffer *buffer,
                                                  uint16_t size)
{
    return buffer->size >= size ? buffer->data : NULL;
}

static inline BteBuffer *bte_buffer_append(BteBuffer *head, BteBuffer *buffer)
{
    if (!head) return bte_buffer_ref(buffer);
    BteBuffer *tail = head;
    while (tail->next) tail = tail->next;
    tail->next = bte_buffer_ref(buffer);
    head->total_size += buffer->size;
    return head;
}

/**
 * @brief Object to write buffer data
 *
 * Objects of this type are generally allocated on the stack and are used to
 * write data into the buffer, handling cases where the data spans across
 * packets.
 *
 * @sa bte_buffer_writer_init()
 */
typedef struct bte_buffer_writer_t {
    BteBuffer *buffer;
    BteBuffer *packet;
    uint16_t pos_in_packet;
    uint8_t header_size;
} BteBufferWriter;

/**
 * @brief Initialized a buffer writer
 *
 * Initializes the memory area pointed by \a writer to start writing data into
 * the buffer \a buffer.
 *
 * @param writer A memory area containing an uninitialized BteBufferWriter
 *        object (can be located on the stack)
 * @param buffer The buffer to write to.
 */
static inline void bte_buffer_writer_init(BteBufferWriter *writer,
                                          BteBuffer *buffer)
{
    writer->buffer = buffer;
    writer->packet = buffer;
    writer->pos_in_packet = 0;
    writer->header_size = 0;
}

/**
 * @brief Set the buffer header size
 *
 * Reserve some bytes at the beginning of the packet for the packet header.
 * When using bte_buffer_writer_write() to write data into the buffer, the
 * header bytes will be skipped.
 *
 * @note Clients shouldn't generally need to call this function, as it's
 * already being called by the library on all buffer writers prepared by the
 * bt-embedded functions.
 *
 * @param writer The buffer writer
 * @param size The header size, in bytes
 *
 * @sa bte_buffer_writer_write()
 */
static inline void bte_buffer_writer_set_header_size(BteBufferWriter *writer,
                                                     uint8_t size)
{
    writer->header_size = size;
    writer->pos_in_packet = size;
}

/**
 * @brief Write data into a buffer
 *
 * Write \a size bytes of \a data into the buffer managed by \a writer.
 * Note that this function does not allocate memory; one should not write more
 * data than what the buffer can allocate.
 *
 * @param writer The buffer writer
 * @param data The data to be written
 * @param size The data size, in bytes
 *
 * @return \c true it all data could be written
 *
 * @sa bte_buffer_writer_init()
 */
static inline bool bte_buffer_writer_write(BteBufferWriter *writer,
                                           const void *data, uint16_t size)
{
    const uint8_t *ptr = (const uint8_t *)data;
    while (size > 0) {
        int write_len = (writer->pos_in_packet + size <= writer->packet->size) ?
            size : (writer->packet->size - writer->pos_in_packet);
        memcpy(writer->packet->data + writer->pos_in_packet, ptr, write_len);
        writer->pos_in_packet += write_len;
        ptr += write_len;
        size -= write_len;
        if (size > 0) {
            /* prepare the next packet */
            if (writer->packet->next) {
                writer->packet = writer->packet->next;
                writer->pos_in_packet = writer->header_size;
            } else {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Get a pointer to the buffer data
 *
 * Get a pointer to the next contiguous area. The current position is not
 * advanced, so make sure to call bte_buffer_writer_advance() after writing
 * some data into the returned pointer, or it will be lost.
 *
 * @param writer The buffer writer
 * @param size Pointer to a variable which will receive the size of the buffer
 *
 * @return A pointer to the buffer data. You can write as much as \a size bytes
 *         into it
 *
 * @sa bte_buffer_writer_advance()
 */
static inline void *bte_buffer_writer_ptr_max(BteBufferWriter *writer,
                                              uint16_t *size)
{
    if (writer->pos_in_packet >= writer->packet->size) {
        /* move to next packet */
        if (writer->packet->next) {
            writer->packet = writer->packet->next;
            writer->pos_in_packet = writer->header_size;
        } else {
            if (size) *size = 0;
            return NULL;
        }
    }

    if (size) *size = writer->packet->size - writer->pos_in_packet;
    return writer->packet->data + writer->pos_in_packet;
}

/**
 * @brief Advances the writer's offset
 *
 * Increases the write offset by \a size. This tells the writer that \a size
 * bytes have been written into the data buffer, and subsequent calls to
 * bte_buffer_writer_ptr_max(), bte_buffer_writer_ptr_n() and
 * bte_buffer_writer_write() will start writing after this offset, and
 * bte_buffer_writer_end() will adjust the buffer size accordingly.
 *
 * @param writer The buffer writer
 * @param size Number of bytes that the offset should be advanced
 *
 * @sa bte_buffer_writer_ptr_max()
 */
static inline void bte_buffer_writer_advance(BteBufferWriter *writer,
                                             uint16_t size)
{
    /* Assume a contiguous block */
    writer->pos_in_packet += size;
}

/**
 * @brief Get a pointer to a contiguous area
 *
 * Get a pointer to the next contiguous area, with the given size; if such a
 * contiguous block does not exist, returns \c NULL.
 * This function advances the current pointer by \a size.
 *
 * @param writer The buffer writer
 * @param size Desired size of the data, in bytes
 *
 * @return A pointer to the data, or \c NULL if the available remaining size of
 *         the packet is less than \a size
 */
static inline void *bte_buffer_writer_ptr_n(BteBufferWriter *writer,
                                            uint16_t size)
{
    if (writer->pos_in_packet >= writer->packet->size) {
        /* move to next packet */
        if (writer->packet->next) {
            writer->packet = writer->packet->next;
            writer->pos_in_packet = writer->header_size;
        } else {
            return NULL;
        }
    }

    if (size > writer->packet->size - writer->pos_in_packet) return NULL;
    uint16_t pos_in_packet = writer->pos_in_packet;
    writer->pos_in_packet += size;
    return writer->packet->data + pos_in_packet;
}

/**
 * @brief Consolidate the writes into the buffer
 *
 * Updates the buffer's size according to the operations that were performed on
 * the \a writer object. The returned buffer can then be sent to a client (for
 * example via bte_l2cap_send_message()).
 *
 * @param writer The buffer writer
 *
 * @return The @ref BteBuffer associated to the writer object
 */
static inline BteBuffer *bte_buffer_writer_end(BteBufferWriter *writer)
{
    writer->packet->size = writer->pos_in_packet;
    /* Recompute the total size */
    uint16_t total_size = 0;
    BteBuffer *buffer = writer->buffer;
    while (buffer) {
        total_size += buffer->size;
        if (buffer == writer->packet) break;
        buffer = buffer->next;
    }
    writer->buffer->total_size = total_size;
    return writer->buffer;
}

/**
 * @defgroup BteBufferReaderG Buffer reader
 * @{
 */

/**
 * @brief Object to read buffer data
 *
 * Objects of this type are generally allocated on the stack and are used to
 * read data from the buffer, handling cases where the data spans across
 * packets.
 *
 * @sa bte_buffer_reader_init()
 */
typedef struct bte_buffer_reader_t {
    BteBuffer *buffer;
    BteBuffer *packet;
    uint16_t pos_in_packet;
    uint8_t header_size;
} BteBufferReader;

/**
 * @brief Initialized a buffer reader
 *
 * Initializes the memory area pointed by \a reader to start reading data from
 * the buffer \a buffer.
 *
 * @param reader A memory area containing an uninitialized BteBufferReader
 *        object (can be located on the stack)
 * @param buffer The buffer to read from.
 */
static inline void bte_buffer_reader_init(BteBufferReader *reader,
                                          BteBuffer *buffer)
{
    reader->buffer = buffer;
    reader->packet = buffer;
    reader->pos_in_packet = 0;
    reader->header_size = 0;
}

/**
 * @brief Set the buffer header size
 *
 * Reserve some bytes at the beginning of the packet for the packet header.
 * When using bte_buffer_reader_read() to read data into the buffer, the header
 * bytes will be skipped.
 *
 * @note Clients shouldn't generally need to call this function, as it's
 * already being called by the library on all buffer readers prepared by the
 * bt-embedded functions.
 *
 * @param reader The buffer reader
 * @param size The header size, in bytes
 *
 * @sa bte_buffer_reader_read()
 */
static inline void bte_buffer_reader_set_header_size(BteBufferReader *reader,
                                                     uint8_t size)
{
    reader->header_size = size;
    reader->pos_in_packet = size;
}

/**
 * @brief Read data from a buffer
 *
 * Read up to \a size bytes from the buffer managed by \a reader and copy them
 * into \a data.
 *
 * @note This function is convenient in that it manages the case where the data
 *       spans across packets, but if you can handle fragmented reads and want
 *       to avoid copies, bte_buffer_reader_read_n() and
 *       bte_buffer_reader_read_max() are more efficient.
 *
 * @param reader The buffer reader
 * @param data Client-allocated buffer where read data will be copied to
 * @param size The maximum data size to read, in bytes
 *
 * @return The amount of data actually read, in bytes. This will always be
 *         equal to the requested amount, unless the buffer is smaller.
 *
 * @sa bte_buffer_reader_init()
 */
static inline uint16_t bte_buffer_reader_read(BteBufferReader *reader,
                                              void *data, uint16_t size)
{
    uint8_t *ptr = (uint8_t *)data;
    uint16_t total_read = 0;
    while (size > 0) {
        int read_len = (reader->pos_in_packet + size <= reader->packet->size) ?
            size : (reader->packet->size - reader->pos_in_packet);
        if (ptr) {
            memcpy(ptr, reader->packet->data + reader->pos_in_packet, read_len);
            ptr += read_len;
        }
        reader->pos_in_packet += read_len;
        total_read += read_len;
        size -= read_len;
        if (size > 0) {
            /* prepare the next packet */
            if (reader->packet->next) {
                reader->packet = reader->packet->next;
                reader->pos_in_packet = reader->header_size;
            } else break;
        }
    }
    return total_read;
}

static inline uint16_t bte_buffer_reader_advance(BteBufferReader *reader,
                                                 uint16_t size)
{
    return bte_buffer_reader_read(reader, NULL, size);
}

/**
 * @brief Get a pointer to the buffer data
 *
 * Get a pointer to the next contiguous area. Note that this also advances the
 * read offset, that is subsequent reads will start reading from the next
 * packet after the current one.
 *
 * @param reader The buffer reader
 * @param size Pointer to a variable which will receive the size of the buffer
 *
 * @return A pointer to the buffer data. You can read as much as \a size bytes
 *         from it. If there is no more data to read, returns \c NULL
 *
 * @sa bte_buffer_reader_read(), bte_buffer_reader_read_n()
 */
static inline void *bte_buffer_reader_read_max(BteBufferReader *reader,
                                               uint16_t *size)
{
    if (reader->pos_in_packet >= reader->packet->size) {
        /* move to next packet */
        if (reader->packet->next) {
            reader->packet = reader->packet->next;
            reader->pos_in_packet = reader->header_size;
        } else {
            if (size) *size = 0;
            return NULL;
        }
    }

    if (size) *size = reader->packet->size - reader->pos_in_packet;
    uint16_t pos_in_packet = reader->pos_in_packet;
    reader->pos_in_packet = reader->packet->size;
    return reader->packet->data + pos_in_packet;
}

/**
 * @brief Get a pointer to the buffer data for the given size
 *
 * Get a pointer to the next contiguous area, to read \a size byte from it.
 *
 * @param reader The buffer reader
 * @param size Number of bytes to read
 *
 * @return A pointer to the buffer data. You can read as much as \a size bytes
 *         from it. If there is no more data to read, returns \c NULL
 *
 * @sa bte_buffer_reader_read(), bte_buffer_reader_read_n()
 */
static inline void *bte_buffer_reader_read_n(BteBufferReader *reader,
                                             uint16_t size)
{
    if (reader->pos_in_packet >= reader->packet->size) {
        /* move to next packet */
        if (reader->packet->next) {
            reader->packet = reader->packet->next;
            reader->pos_in_packet = reader->header_size;
        } else {
            return NULL;
        }
    }

    if (size > reader->packet->size - reader->pos_in_packet) return NULL;
    uint16_t pos_in_packet = reader->pos_in_packet;
    reader->pos_in_packet += size;
    return reader->packet->data + pos_in_packet;
}

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BTE_BUFFER_H */
