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

static inline void *bte_malloc(uint16_t size)
{
#ifdef BTE_BUFFER_ALIGNMENT_SIZE
    return memalign(BTE_BUFFER_ALIGNMENT_SIZE,
                    (size + BTE_BUFFER_ALIGNMENT_SIZE - 1) &
                    ~(BTE_BUFFER_ALIGNMENT_SIZE - 1));
#else
    return malloc(size);
#endif
}

static void bte_buffer_unref(BteBuffer *buffer);

/* Used for small packets (TODO: clarify) */
static inline BteBuffer *bte_buffer_alloc_contiguous(uint16_t size)
{
    BteBuffer *b = (BteBuffer*)bte_malloc(sizeof(BteBuffer) + size);
    b->ref_count = 1;
    b->free_func = (void (*)(BteBuffer *))free;
    b->total_size = b->size = size;
    b->next = NULL;
    return b;
}

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

static inline BteBuffer *bte_buffer_ref(BteBuffer *buffer)
{
    atomic_fetch_add(&buffer->ref_count, 1);
    return buffer;
}

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
    if (!head) return buffer;
    BteBuffer *tail = head;
    while (tail->next) tail = tail->next;
    tail->next = bte_buffer_ref(buffer);
    head->total_size += buffer->size;
    return head;
}

typedef struct bte_buffer_writer_t {
    BteBuffer *buffer;
    BteBuffer *packet;
    uint16_t pos_in_packet;
    uint8_t header_size;
} BteBufferWriter;

static inline void bte_buffer_writer_init(BteBufferWriter *writer,
                                          BteBuffer *buffer)
{
    writer->buffer = buffer;
    writer->packet = buffer;
    writer->pos_in_packet = 0;
    writer->header_size = 0;
}

static inline void bte_buffer_writer_set_header_size(BteBufferWriter *writer,
                                                     uint8_t size)
{
    writer->header_size = size;
    writer->pos_in_packet = size;
}

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

/*!
 * Get a pointer to the next contiguous area. The current position is not
 * advanced (see bte_buffer_writer_advance() for that).
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

static inline void bte_buffer_writer_advance(BteBufferWriter *writer,
                                             uint16_t size)
{
    /* Assume a contiguous block */
    writer->pos_in_packet += size;
}

/*!
 * Get a pointer to the next contiguous area, with the given size; if such a
 * contiguous block does not exist, returns NULL.
 * This function advances the current pointer by \a size.
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

typedef struct bte_buffer_reader_t {
    BteBuffer *buffer;
    BteBuffer *packet;
    uint16_t pos_in_packet;
    uint8_t header_size;
} BteBufferReader;

static inline void bte_buffer_reader_init(BteBufferReader *reader,
                                          BteBuffer *buffer)
{
    reader->buffer = buffer;
    reader->packet = buffer;
    reader->pos_in_packet = 0;
    reader->header_size = 0;
}

static inline void bte_buffer_reader_set_header_size(BteBufferReader *reader,
                                                     uint8_t size)
{
    reader->header_size = size;
    reader->pos_in_packet = size;
}

static inline uint16_t bte_buffer_reader_read(BteBufferReader *reader,
                                              void *data, uint16_t size)
{
    uint8_t *ptr = (uint8_t *)data;
    uint16_t total_read = 0;
    while (size > 0) {
        int read_len = (reader->pos_in_packet + size <= reader->packet->size) ?
            size : (reader->packet->size - reader->pos_in_packet);
        memcpy(ptr, reader->packet->data + reader->pos_in_packet, read_len);
        reader->pos_in_packet += read_len;
        ptr += read_len;
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

/*! Get a pointer to the next contiguous area */
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

/*!
 * Get a pointer to the next contiguous area, with the given size; if such a
 * contiguous block does not exist, returns NULL.
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

#ifdef __cplusplus
}
#endif

#endif /* BTE_BUFFER_H */
