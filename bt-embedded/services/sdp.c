#include "sdp.h"

#include "buffer.h"
#include "l2cap.h"
#include "logging.h"
#include "utils.h"

#include <stdarg.h>
#include <stdlib.h>

#define BTE_SDP_DE_SIZE_MASK      (BteSdpDeType)0x07
#define BTE_SDP_DE_SIZE_1      0
#define BTE_SDP_DE_SIZE_2      1
#define BTE_SDP_DE_SIZE_4      2
#define BTE_SDP_DE_SIZE_8      3
#define BTE_SDP_DE_SIZE_16     4
#define BTE_SDP_DE_SIZE_VAR_8  5
#define BTE_SDP_DE_SIZE_VAR_16 6
#define BTE_SDP_DE_SIZE_VAR_32 7

#define PDU_HEADER_SIZE (1 + 2 + 2)

#define PDU_ID_ERROR_RSP               0x01
#define PDU_ID_SERVICE_SEARCH_REQ      0x02
#define PDU_ID_SERVICE_SEARCH_RSP      0x03
#define PDU_ID_SERVICE_ATTR_REQ        0x04
#define PDU_ID_SERVICE_ATTR_RSP        0x05
#define PDU_ID_SERVICE_SEARCH_ATTR_REQ 0x06
#define PDU_ID_SERVICE_SEARCH_ATTR_RSP 0x07

/* The offsets are relative to the raw ACL packet */
#define PDU_OFFSET_OPCODE     8
#define PDU_OFFSET_TRANS_ID   9
#define PDU_OFFSET_PARAM_LEN 11
#define PDU_OFFSET_PARAMS    13

#ifdef __SIZEOF_INT128__
static inline BteSdpUint128 uint128_from_64(uint64_t v) {
    return v;
}
static inline BteSdpInt128 int128_from_64(int64_t v) {
    return v;
}
#else
static inline BteSdpUint128 uint128_from_64(uint64_t v) {
    BteSdpUint128 ret;
#if __BYTE_ORDER == __LITTLE_ENDIAN
    ret.parts[0] = v; ret.parts[1] = 0;
#else
    ret.parts[1] = v; ret.parts[0] = 0;
#endif
    return ret;
}
static inline BteSdpInt128 int128_from_64(int64_t v) {
    BteSdpUint128 ret;
#if __BYTE_ORDER == __LITTLE_ENDIAN
    ret.parts[0] = (uint64_t)v; ret.parts[1] = (uint64_t)(v > 0 ? 0 : -1);
#else
    ret.parts[1] = (uint64_t)v; ret.parts[0] = (uint64_t)(v > 0 ? 0 : -1);
#endif
    return ret;
}
#endif

struct bte_sdp_client_t {
    atomic_int ref_count;
    void *userdata;

    BteL2cap *l2cap;

    BteBuffer *last_req;
    uint16_t continuation_offset;

    union _bte_sdp_client_last_async_req_u {
        struct {
            BteSdpServiceSearchCb cb;
            void *userdata;
        } service_search;
        struct {
            BteSdpServiceAttrCb cb;
            void *userdata;
            uint8_t *attr_list_de;
            uint16_t written;
        } service_attr;
    } req_data;
};

static const BteSdpDeUuid128 s_base_uuid = {{
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x10, 0x00,
    0x80, 0x00,
    0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb
}};

static uint32_t s_next_transaction_id = 0;

static inline uint16_t next_transaction_id()
{
    return s_next_transaction_id++;
}

static BteSdpUint128 read_be128(const void *data)
{
    const uint8_t *d = data;
    BteSdpUint128 ret;
    memcpy(&ret, d, sizeof(ret));
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint64_t *parts = (uint64_t*)&ret;
    uint64_t tmp = parts[0];
    write_be64(parts[1], &parts[0]);
    write_be64(tmp, &parts[1]);
#endif
    return ret;
}

uint32_t bte_sdp_de_get_data_size(const uint8_t *de)
{
    uint8_t size_id = de[0] & BTE_SDP_DE_SIZE_MASK;
    switch (size_id) {
    case BTE_SDP_DE_SIZE_VAR_8:
        return de[1];
    case BTE_SDP_DE_SIZE_VAR_16:
        return read_be16(de + 1);
    case BTE_SDP_DE_SIZE_VAR_32:
        return read_be32(de + 1);
    default:
        if (de[0] == BTE_SDP_DE_TYPE_NIL) return 0;
        return 1 << size_id;
    }
}

uint32_t bte_sdp_de_get_header_size(const uint8_t *de)
{
    uint8_t size_id = de[0] & BTE_SDP_DE_SIZE_MASK;
    switch (size_id) {
    case BTE_SDP_DE_SIZE_VAR_8:
        return 1 + 1;
    case BTE_SDP_DE_SIZE_VAR_16:
        return 1 + 2;
    case BTE_SDP_DE_SIZE_VAR_32:
        return 1 + 4;
    default:
        return 1;
    }
}

uint32_t bte_sdp_de_get_total_size(const uint8_t *de)
{
    return bte_sdp_de_get_header_size(de) + bte_sdp_de_get_data_size(de);
}

BteSdpDeType bte_sdp_de_get_type(const uint8_t *de)
{
    uint8_t type_id = de[0] & BTE_SDP_DE_TYPE_MASK;
    return type_id >= BTE_SDP_DE_TYPE_STRING ? type_id : de[0];
}

static uint16_t cont_state_len(const uint8_t *cont_state)
{
    return 1 + (cont_state ? cont_state[0] : 0);
}

static bool write_cont_state(BteBufferWriter *writer,
                             const uint8_t *cont_state)
{
    static uint8_t nil = 0;
    return bte_buffer_writer_write(writer,
                                   cont_state ? cont_state : &nil,
                                   1 + (cont_state ? cont_state[0] : 0));
}

static uint8_t var_size(size_t len, int *header_size, uint8_t *dest)
{
    uint8_t size_id;
    if (len < 0x100) {
        size_id = BTE_SDP_DE_SIZE_VAR_8;
        if (header_size) *header_size = 1 + 1;
        if (dest) dest[1] = len;
    } else if (len < 0x10000) {
        size_id = BTE_SDP_DE_SIZE_VAR_16;
        if (header_size) *header_size = 1 + 2;
        if (dest) write_be16(len, dest + 1);
    } else {
        size_id = BTE_SDP_DE_SIZE_VAR_32;
        if (header_size) *header_size = 1 + 4;
        if (dest) write_be32(len, dest + 1);
    }
    return size_id;
}

static ssize_t de_array_size(int count, int type, const void *data)
{
    ssize_t size;
    if ((type >= BTE_SDP_DE_TYPE_NIL && type <= BTE_SDP_DE_TYPE_UUID128) ||
        type == BTE_SDP_DE_TYPE_BOOL) {
        /* Fixed length type */
        uint8_t type_id = type & 0xff;
        size = bte_sdp_de_get_total_size(&type_id) * count;
    } else if (type == BTE_SDP_DE_TYPE_STRING || type == BTE_SDP_DE_TYPE_URL) {
        const char *const *strings = data;
        size = 0;
        for (int i = 0; i < count; i++) {
            size_t len = strlen(strings[i]);
            int header_size = 0;
            var_size(len, &header_size, NULL);
            size += header_size + len;
        }
    } else {
        /* We currently do not support arrays of sequences */
        assert(false);
        size = 0; /* Make the compiler happy */
    }
    return size;
}

/* This function assumes that there's enough room for storing the array */
static void de_array_write(uint8_t *de, int count, int type, const void *data)
{
    if ((type >= BTE_SDP_DE_TYPE_NIL && type <= BTE_SDP_DE_TYPE_UUID128) ||
        type == BTE_SDP_DE_TYPE_BOOL) {
        uint8_t size_id = type & BTE_SDP_DE_SIZE_MASK;
        /* Fixed size: we need to copy the data paying attention to the
         * endianness */
        const uint8_t *src = data;
        int size = 1 << size_id;
        for (int i = 0; i < count; i++) {
            *(de++) = type;
            for (int j = 0; j < size; j++) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
                *(de++) = src[size - 1 - j];
#else
                *(de++) = src[j];
#endif
            }
            src += size;
        }
    } else if (type == BTE_SDP_DE_TYPE_STRING || type == BTE_SDP_DE_TYPE_URL) {
        const char *const *strings = data;
        for (int i = 0; i < count; i++) {
            size_t len = strlen(strings[i]);
            int header_size = 0;
            uint8_t size_id = var_size(len, &header_size, de);
            de[0] = type | size_id;
            de += header_size;
            strcpy((char *)de, strings[i]);
            de += len;
        }
    }
}

static uint32_t de_write(int depth, uint8_t *de, size_t buffer_size,
                         va_list *args)
{
    uint32_t size = 0;
    uint8_t *dest = de;
    ssize_t remaining = de ? buffer_size : 0;
    int type = -1;
    while (true) {
        type = va_arg(*args, int);
        if (type == BTE_SDP_DE_END) break;

        bool flip = false;
        ssize_t req_size = 1;
        uint8_t v8;
        uint16_t v16;
        uint32_t v32;
        uint64_t v64;
        BteSdpUint128 v128;
        switch (type) {
        case BTE_SDP_DE_TYPE_NIL:
            if (remaining > req_size) {
                dest[0] = type;
            }
            break;
        case BTE_SDP_DE_TYPE_INT8:
        case BTE_SDP_DE_TYPE_UINT8:
            req_size = 1 + 1;
            v8 = (uint8_t)va_arg(*args, int);
            if (remaining > req_size) {
                dest[0] = type;
                dest[1] = v8;
            }
            break;
        case BTE_SDP_DE_TYPE_INT16:
        case BTE_SDP_DE_TYPE_UINT16:
        case BTE_SDP_DE_TYPE_UUID16:
            req_size = 1 + 2;
            v16 = va_arg(*args, int);
            if (remaining > req_size) {
                dest[0] = type;
                write_be16(v16, dest + 1);
            }
            break;
        case BTE_SDP_DE_TYPE_INT32:
        case BTE_SDP_DE_TYPE_UINT32:
        case BTE_SDP_DE_TYPE_UUID32:
            req_size = 1 + 4;
            v32 = va_arg(*args, uint32_t);
            if (remaining > req_size) {
                dest[0] = type;
                write_be32(v32, dest + 1);
            }
            break;
        case BTE_SDP_DE_TYPE_INT64:
        case BTE_SDP_DE_TYPE_UINT64:
            v64 = va_arg(*args, uint64_t);
            req_size = 1 + 8;
            if (remaining > req_size) {
                dest[0] = type;
                write_be64(v64, dest + 1);
            }
            break;
        case BTE_SDP_DE_TYPE_INT128:
        case BTE_SDP_DE_TYPE_UINT128:
#if __BYTE_ORDER == __LITTLE_ENDIAN
            flip = true;
            // fallthrough
#endif
        case BTE_SDP_DE_TYPE_UUID128:
            req_size = 1 + 16;
            v128 = va_arg(*args, BteSdpUint128);
            if (remaining > req_size) {
                dest[0] = type;
                memcpy(dest + 1, &v128, sizeof(v128));
                if (flip) {
                    uint64_t tmp = *((uint64_t*)(dest + 1));
                    write_be64(*((uint64_t*)(dest + 1 + 8)), dest + 1);
                    write_be64(tmp, dest + 1 + 8);
                }
            }
            break;
        case BTE_SDP_DE_TYPE_BOOL:
            req_size = 1 + 1;
            v8 = va_arg(*args, int) != 0 ? 1 : 0;
            if (remaining > req_size) {
                dest[0] = type;
                dest[1] = v8;
            }
            break;
        case BTE_SDP_DE_TYPE_STRING:
        case BTE_SDP_DE_TYPE_URL:
            {
                const char *str = va_arg(*args, char *);
                size_t len = strlen(str);
                int header_size;
                uint8_t size_id = var_size(len, &header_size, NULL);
                req_size = header_size + len;
                if (remaining > req_size) {
                    dest[0] = type | size_id;
                    var_size(len, NULL, dest);
                    memcpy(dest + header_size, str, len);
                }
            }
            break;
        case BTE_SDP_DE_TYPE_SEQUENCE:
        case BTE_SDP_DE_TYPE_CHOICE:
            {
                /* First, we need to compute the size of the included data */
                va_list args_copy;
                va_copy(args_copy, *args);
                uint32_t len = de_write(depth + 1, NULL, 0, args);
                int header_size;
                uint8_t size_id = var_size(len, &header_size, NULL);
                req_size = header_size + len;
                if (remaining > req_size) {
                    dest[0] = type | size_id;
                    var_size(len, NULL, dest);
                    de_write(depth + 1, dest + header_size,
                             remaining - header_size, &args_copy);
                }
                va_end(args_copy);
            }
            break;
        default:
            if ((type & BTE_SDP_DE_SPECIAL_MASK) == BTE_SDP_DE_SPECIAL_ARRAY) {
                int count = type & (~BTE_SDP_DE_SPECIAL_MASK);
                type = va_arg(*args, int);
                /* We assume that the caller is sane and specifies a basic type
                 */
                void *elements = va_arg(*args, void *);
                req_size = de_array_size(count, type, elements);
                if (remaining > req_size) {
                    de_array_write(dest, count, type, elements);
                }
            }
        }
        if (dest) dest += req_size;
        size += req_size;
        remaining -= req_size;
        if (depth == 0) break;
    }

    return size;
}

uint32_t bte_sdp_de_write(uint8_t *de, size_t buffer_size, ...)
{
    va_list args;
    va_start(args, buffer_size);
    uint32_t size = de_write(0, de, buffer_size, &args);
    va_end(args);
    return size;
}

void bte_sdp_de_reader_init(BteSdpDeReader *reader, const uint8_t *de)
{
    reader->de = de;
    reader->offset = 0;
    reader->seq_start_offset = 0;
    reader->depth = 0;
    reader->next_called = false;
}

bool bte_sdp_de_reader_next(BteSdpDeReader *reader)
{
    if (UNLIKELY(reader->depth == 0)) return false;

    uint32_t seq_size = bte_sdp_de_get_total_size(reader->de +
                                                  reader->seq_start_offset);
    uint32_t end = reader->seq_start_offset + seq_size;
    if (reader->offset >= end) return false;

    if (!reader->next_called) {
        reader->next_called = true;
        return true;
    }
    uint32_t el_size = bte_sdp_de_get_total_size(reader->de + reader->offset);
    reader->offset += el_size;
    return reader->offset < end;
}

bool bte_sdp_de_reader_enter(BteSdpDeReader *reader)
{
    const uint8_t *de = reader->de + reader->offset;
    BteSdpDeType type = bte_sdp_de_get_type(de);
    if (type != BTE_SDP_DE_TYPE_SEQUENCE && type != BTE_SDP_DE_TYPE_CHOICE) {
        return false;
    }

    reader->seq_start_offset = reader->offset;
    reader->offset += bte_sdp_de_get_header_size(de);
    reader->depth++;
    reader->next_called = false;
    return true;
}

static int find_seq_start_offset(const uint8_t *de, uint8_t depth,
                                 int contained_offset)
{
    BteSdpDeReader reader;
    bte_sdp_de_reader_init(&reader, de);
    do {
        uint32_t size = bte_sdp_de_reader_get_total_size(&reader);
        if (reader.offset + size >= contained_offset) {
            if (reader.depth == depth) {
                return reader.offset;
            } else {
                /* This *must* be a sequence! */
                bool ok = bte_sdp_de_reader_enter(&reader);
                assert(ok);
                UNUSED(ok);
            }
        } else {
            bte_sdp_de_reader_next(&reader);
        }
    } while (true);
}

bool bte_sdp_de_reader_leave(BteSdpDeReader *reader)
{
    if (reader->depth == 0) return false;

    reader->offset = reader->seq_start_offset;
    reader->depth--;
    if (reader->depth == 0) {
        reader->seq_start_offset = 0;
    } else {
        /* Re-scan the DE from the beginning, to figure out if we are still
         * inside a sequence and update seq_start_offset accordingly */
        reader->seq_start_offset =
            find_seq_start_offset(reader->de, reader->depth - 1, reader->offset);
    }
    return true;
}

static const uint8_t *de_get_current(BteSdpDeReader *reader)
{
    if (reader->depth > 0) {
        if (UNLIKELY(!reader->next_called)) return NULL;
        int seq_size = bte_sdp_de_get_total_size(reader->de +
                                                 reader->seq_start_offset);
        if (UNLIKELY(reader->offset >= reader->seq_start_offset + seq_size)) {
            return NULL;
        }
    }

    return reader->de + reader->offset;
}

static const uint8_t *de_get_data(BteSdpDeReader *reader, uint32_t *size)
{
    const uint8_t *de = de_get_current(reader);
    if (UNLIKELY(!de)) return NULL;
    *size = bte_sdp_de_get_data_size(de);
    return *size > 0 ? (de + bte_sdp_de_get_header_size(de)): NULL;
}

BteSdpDeType bte_sdp_de_reader_get_type(BteSdpDeReader *reader)
{
    const uint8_t *de = de_get_current(reader);
    if (UNLIKELY(!de)) return BTE_SDP_DE_TYPE_INVALID;

    return bte_sdp_de_get_type(de);
}

uint32_t bte_sdp_de_reader_get_total_size(BteSdpDeReader *reader)
{
    const uint8_t *de = de_get_current(reader);
    if (UNLIKELY(!de)) return 0;

    return bte_sdp_de_get_total_size(de);
}

uint8_t bte_sdp_de_reader_read_uint8(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size != 1)) return 0;

    return data[0];
}

uint16_t bte_sdp_de_reader_read_uint16(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 2)) return 0;

    return size == 2 ? read_be16(data) : data[0];
}

int16_t bte_sdp_de_reader_read_int16(BteSdpDeReader *reader) {
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 2)) return 0;

    return size == 2 ? (int16_t)read_be16(data) : (int8_t)data[0];
}

uint32_t bte_sdp_de_reader_read_uint32(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size == 3 || size > 4)) return 0;

    return size == 4 ? read_be32(data) :
        (size == 2 ? read_be16(data) : data[0]);
}

int32_t bte_sdp_de_reader_read_int32(BteSdpDeReader *reader) {
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 4)) return 0;

    return size == 4 ? (int32_t)read_be32(data) :
        bte_sdp_de_reader_read_int16(reader);
}

uint64_t bte_sdp_de_reader_read_uint64(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 8)) return 0;

    return size == 8 ? read_be64(data) : bte_sdp_de_reader_read_uint32(reader);
}

int64_t bte_sdp_de_reader_read_int64(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 8)) return 0;

    return size == 8 ? (int64_t)read_be64(data) :
        bte_sdp_de_reader_read_int32(reader);
}

BteSdpUint128 bte_sdp_de_reader_read_uint128(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 16)) return uint128_from_64(0);

    return size == 16 ? read_be128(data) :
        uint128_from_64(bte_sdp_de_reader_read_uint64(reader));
}

BteSdpInt128 bte_sdp_de_reader_read_int128(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data || size > 16)) return int128_from_64(0);

    return size == 16 ? (BteSdpInt128)read_be128(data) :
        int128_from_64(bte_sdp_de_reader_read_int64(reader));
}

BteSdpDeUuid128 bte_sdp_de_reader_read_uuid128(BteSdpDeReader *reader)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    BteSdpDeUuid128 ret;
    memset(&ret, 0, sizeof(ret));
    if (UNLIKELY(!data)) return ret;

    if (size == 16) {
        memcpy(&ret, data, sizeof(ret));
    } else if (size <=4) {
        uint32_t uuid32 = bte_sdp_de_reader_read_uuid32(reader);
        if (uuid32 != 0) {
            memcpy(&ret, &s_base_uuid, sizeof(ret));
            write_be32(uuid32, &ret);
        }
    }
    return ret;
}

const char *bte_sdp_de_reader_read_str(BteSdpDeReader *reader, size_t *len)
{
    uint32_t size;
    const uint8_t *data = de_get_data(reader, &size);
    if (UNLIKELY(!data)) return NULL;

    const uint8_t *de = reader->de + reader->offset;
    BteSdpDeType type = bte_sdp_de_get_type(de);
    if (UNLIKELY(type != BTE_SDP_DE_TYPE_STRING &&
                 type != BTE_SDP_DE_TYPE_URL )) return NULL;

    if (len) *len = size;
    return (const char *)data;
}

size_t bte_sdp_de_reader_copy_str(BteSdpDeReader *reader,
                                  char *buffer, size_t max_len)
{
    size_t len;
    const char *str = bte_sdp_de_reader_read_str(reader, &len);
    if (LIKELY(str)) {
        return snprintf(buffer, max_len, "%.*s", (int)len, str);
    } else {
        if (max_len > 0) buffer[0] = '\0';
        return 0;
    }
}

static bool create_pdu(BteL2cap *l2cap, BteBufferWriter *writer,
                       uint8_t code, uint16_t id, uint16_t param_len)
{
    bool ok = bte_l2cap_create_message(l2cap, writer,
                                       PDU_HEADER_SIZE + param_len);
    if (UNLIKELY(!ok)) return false;

    uint8_t *hdr = bte_buffer_writer_ptr_n(writer, PDU_HEADER_SIZE);
    hdr[0] = code;
    write_be16(id, hdr + 1);
    write_be16(param_len, hdr + 3);
    return true;
}

static bool send_continuation_request(BteSdpClient *sdp,
                                      const uint8_t *cont_state)
{
    uint8_t req_opcode = sdp->last_req->data[PDU_OFFSET_OPCODE];

    uint16_t data_len = sdp->continuation_offset;
    uint16_t size = data_len + cont_state_len(cont_state);
    BteBufferWriter writer;
    bool ok = create_pdu(sdp->l2cap, &writer, req_opcode,
                         next_transaction_id(), size);
    if (UNLIKELY(!ok)) return false;

    void *dest = bte_buffer_writer_ptr_n(&writer, sdp->continuation_offset);
    memcpy(dest, sdp->last_req->data + PDU_OFFSET_PARAMS, data_len);
    ok = write_cont_state(&writer, cont_state);
    if (UNLIKELY(!ok)) goto error;

    bte_buffer_unref(sdp->last_req);
    sdp->last_req = bte_buffer_ref(bte_buffer_writer_end(&writer));
    int rc = bte_l2cap_send_message(sdp->l2cap, sdp->last_req);
    return rc >= 0;

error:
    bte_buffer_unref(writer.buffer);
    return NULL;
}

static bool parse_service_search_reply(
    uint8_t *params, uint16_t param_len, BteSdpServiceSearchReply *reply,
    uint8_t **cont_state)
{
    if (UNLIKELY(param_len < 4)) return false;

    uint16_t remaining = param_len;
    reply->total_count = read_be16(params);
    reply->count = read_be16(params + 2);
    uint16_t read = 4;
    remaining -= read;
    params += read;

    if (UNLIKELY(remaining < reply->count * 4)) return false;

#if __BYTE_ORDER == __LITTLE_ENDIAN
    for (int i = 0; i < reply->count; i++) {
        /* We modify the data in place */
        uint32_t handle = read_be32(params + i * 4);
        *(uint32_t*)(params + i * 4) = handle;
    }
#endif
    reply->handles = (uint32_t*)params;
    read = reply->count * 4;
    params += read;
    remaining -= read;

    if (UNLIKELY(remaining < 1)) return false;
    uint16_t len = cont_state_len(params);
    if (UNLIKELY(len > 17 || len > remaining)) return false;

    *cont_state = params;
    reply->has_more = params[0] != 0;
    return true;
}

static bool parse_service_attr_reply(
    BteSdpClient *sdp, uint8_t *params, uint16_t param_len,
    BteSdpServiceAttrReply *reply, uint8_t **cont_state)
{
    if (UNLIKELY(param_len < 3)) return false;

    uint16_t remaining = param_len;
    uint16_t attrs_size = read_be16(params);
    uint16_t read = 2;
    params += read;
    remaining -= read;

    const uint8_t *id_list = params;
    read = attrs_size;
    params += read;
    remaining -= read;

    if (UNLIKELY(remaining < 1)) return false;
    uint16_t len = cont_state_len(params);
    if (UNLIKELY(len > 17 || len > remaining)) return false;

    *cont_state = params;
    uint32_t total_size, written;
    if (params[0] != 0 || sdp->req_data.service_attr.attr_list_de) {
        /* The reply is fragmented */
        if (!sdp->req_data.service_attr.attr_list_de) {
            total_size = bte_sdp_de_get_total_size(id_list);
            if (total_size >= 0x10000) return false;

            sdp->req_data.service_attr.attr_list_de = bte_malloc(total_size);
        } else {
            total_size = bte_sdp_de_get_total_size(
                sdp->req_data.service_attr.attr_list_de);
        }

        if (sdp->req_data.service_attr.written + attrs_size > total_size) {
            return false;
        }

        memcpy(sdp->req_data.service_attr.attr_list_de +
               sdp->req_data.service_attr.written, id_list, attrs_size);
        sdp->req_data.service_attr.written += attrs_size;
        reply->attr_list_de = sdp->req_data.service_attr.attr_list_de;
        written = sdp->req_data.service_attr.written;
    } else {
        total_size = bte_sdp_de_get_total_size(id_list);
        written = attrs_size;
        reply->attr_list_de = id_list;
    }

    if (params[0] == 0 &&
        /* This is the last packet: check that the DE size matches */
        written != total_size) {
        return false;
    }
    return true;
}

static void on_message_received(BteL2cap *l2cap, BteBufferReader *reader,
                                void *userdata)
{
    BteSdpClient *sdp = userdata;

    if (UNLIKELY(!sdp->last_req)) {
        /* Unsolicited message, ignore it */
        return;
    }

    uint8_t req_opcode = sdp->last_req->data[PDU_OFFSET_OPCODE];
    /* This is in BE format, but here we don't care, since we will be comparing
     * the raw values */
    uint16_t req_id = *(uint16_t*)(sdp->last_req->data + PDU_OFFSET_TRANS_ID);

    uint8_t *hdr = bte_buffer_reader_read_n(reader, PDU_HEADER_SIZE);
    uint8_t rsp_opcode = hdr[0];
    uint16_t rsp_id = *(uint16_t*)(hdr + 1);
    if (rsp_id != req_id ||
        (rsp_opcode != req_opcode + 1 && rsp_opcode != PDU_ID_ERROR_RSP)) {
        /* The incoming message does not match our request: ignore it */
        return;
    }

    uint16_t error_code = 0;
    if (UNLIKELY(rsp_opcode == PDU_ID_ERROR_RSP)) {
        uint8_t *params = bte_buffer_reader_read_n(reader, 2);
        error_code = read_be16(params);
        if (UNLIKELY(error_code == 0)) {
            /* Invalid packet, ignore it */
            return;
        }
    }

    uint16_t param_len = read_be16(hdr + 3);
    uint8_t *params = bte_buffer_reader_read_n(reader, param_len);
    void *params_buffer = NULL;
    if (UNLIKELY(!params)) {
        /* The buffer is fragmented, let's copy its data */
        params_buffer = bte_malloc(param_len);
        if (UNLIKELY(!params_buffer)) return;

        bte_buffer_reader_read(reader, params_buffer, param_len);
        params = params_buffer;
    }

    /* We might be invoking user callbacks, and they might unref our object.
     * Keep a temporary reference to it */
    bte_sdp_client_ref(sdp);

    uint8_t *cont_state = NULL;
    bool req_complete = false;
    if (req_opcode == PDU_ID_SERVICE_SEARCH_REQ) {
        BteSdpServiceSearchCb cb = sdp->req_data.service_search.cb;

        BteSdpServiceSearchReply reply;
        reply.error_code = error_code;
        if (UNLIKELY(error_code != 0)) {
            reply.total_count = reply.count = 0;
            reply.has_more = false;
            reply.handles = NULL;
            req_complete = true;
        } else {
            bool ok = parse_service_search_reply(params, param_len,
                                                 &reply, &cont_state);
            if (UNLIKELY(!ok)) {
                goto end;
            }
            req_complete = !reply.has_more;
        }
        bool wants_more =
            cb(sdp, &reply, sdp->req_data.service_search.userdata);
        if (!wants_more) req_complete = true;
    } else if (req_opcode == PDU_ID_SERVICE_ATTR_REQ ||
               req_opcode == PDU_ID_SERVICE_SEARCH_ATTR_REQ) {
        BteSdpServiceAttrCb cb = sdp->req_data.service_attr.cb;

        BteSdpServiceAttrReply reply;
        reply.error_code = error_code;
        if (UNLIKELY(error_code != 0)) {
            reply.attr_list_de = NULL;
            req_complete = true;
        } else {
            bool ok = parse_service_attr_reply(sdp, params, param_len,
                                               &reply, &cont_state);
            if (UNLIKELY(!ok)) {
                goto end;
            }
            req_complete = cont_state[0] == 0;
        }
        if (req_complete) {
            cb(sdp, &reply, sdp->req_data.service_attr.userdata);
            if (sdp->req_data.service_attr.attr_list_de) {
                bte_free(sdp->req_data.service_attr.attr_list_de);
            }
        }
    }

    if (req_complete) {
        bte_buffer_unref(sdp->last_req);
        sdp->last_req = NULL;
    } else {
        send_continuation_request(sdp, cont_state);
    }
end:
    if (params_buffer) bte_free(params_buffer);
    bte_sdp_client_unref(sdp); /* temp reference */
}

static void bte_sdp_client_free(BteSdpClient *sdp)
{
    bte_l2cap_unref(sdp->l2cap);
    bte_free(sdp);
}

BteSdpClient *bte_sdp_client_new(BteL2cap *l2cap)
{
    BteSdpClient *sdp = bte_malloc(sizeof(BteSdpClient));
    memset(sdp, 0, sizeof(BteSdpClient));
    sdp->ref_count = 1;
    sdp->l2cap = bte_l2cap_ref(l2cap);
    sdp->userdata = bte_l2cap_get_userdata(l2cap);
    bte_l2cap_set_userdata(l2cap, sdp);
    bte_l2cap_on_message_received(l2cap, on_message_received);
    return sdp;
}

BteSdpClient *bte_sdp_client_ref(BteSdpClient *sdp)
{
    atomic_fetch_add(&sdp->ref_count, 1);
    return sdp;
}

void bte_sdp_client_unref(BteSdpClient *sdp)
{
    if (atomic_fetch_sub(&sdp->ref_count, 1) == 1) {
        bte_sdp_client_free(sdp);
    }
}

void bte_sdp_client_set_userdata(BteSdpClient *sdp, void *userdata)
{
    sdp->userdata = userdata;
}

void *bte_sdp_client_get_userdata(BteSdpClient *sdp)
{
    return sdp->userdata;
}


BteL2cap *bte_sdp_client_get_l2cap(BteSdpClient *sdp)
{
    return sdp->l2cap;
}

static BteBuffer *service_search_req_packet(
    BteSdpClient *sdp, const uint8_t *pattern, uint16_t max_count)
{
    uint32_t pattern_size = bte_sdp_de_get_total_size(pattern);
    uint16_t size = pattern_size + 2 + 1;

    BteBufferWriter writer;
    bool ok = create_pdu(sdp->l2cap, &writer, PDU_ID_SERVICE_SEARCH_REQ,
                         next_transaction_id(), size);
    if (UNLIKELY(!ok)) return NULL;

    ok = bte_buffer_writer_write(&writer, pattern, pattern_size);
    if (UNLIKELY(!ok)) goto error;

    uint16_t max_count_be = htobe16(max_count);
    ok = bte_buffer_writer_write(&writer, &max_count_be, sizeof(max_count_be));
    if (UNLIKELY(!ok)) goto error;

    sdp->continuation_offset = pattern_size + 2;
    ok = write_cont_state(&writer, NULL);
    if (UNLIKELY(!ok)) goto error;

    return bte_buffer_writer_end(&writer);

error:
    bte_buffer_unref(writer.buffer);
    return NULL;
}

bool bte_sdp_service_search_req(BteSdpClient *sdp, const uint8_t *pattern,
                                uint16_t max_count,
                                BteSdpServiceSearchCb cb, void *userdata)
{
    if (sdp->last_req) return false;

    BteBuffer *buffer = service_search_req_packet(sdp, pattern, max_count);
    if (UNLIKELY(!buffer)) return false;

    sdp->last_req = bte_buffer_ref(buffer);
    int rc = bte_l2cap_send_message(sdp->l2cap, buffer);
    if (UNLIKELY(rc < 0)) {
        bte_buffer_unref(sdp->last_req);
        sdp->last_req = NULL;
        return false;
    }

    sdp->req_data.service_search.cb = cb;
    sdp->req_data.service_search.userdata = userdata;
    return true;
}

bool bte_sdp_service_search_req_uuid16(
    BteSdpClient *sdp, uint16_t *uuids, int n_uuids, uint16_t max_count,
    BteSdpServiceSearchCb cb, void *userdata)
{
    uint8_t buffer[32];

    if (UNLIKELY(n_uuids > 12)) return false;
    bte_sdp_de_write(buffer, sizeof(buffer), BTE_SDP_DE_TYPE_SEQUENCE,
                     BTE_SDP_DE_ARRAY(n_uuids), BTE_SDP_DE_TYPE_UUID16,
                     uuids, BTE_SDP_DE_END, BTE_SDP_DE_END);

    return bte_sdp_service_search_req(sdp, buffer, max_count, cb, userdata);
}

static BteBuffer *service_attr_req_packet(
    BteSdpClient *sdp, uint32_t service_record, uint16_t max_count,
    const uint8_t *id_list)
{
    uint32_t id_list_size = bte_sdp_de_get_total_size(id_list);
    uint16_t size = 4 + 2 + id_list_size + 1;

    BteBufferWriter writer;
    bool ok = create_pdu(sdp->l2cap, &writer, PDU_ID_SERVICE_ATTR_REQ,
                         next_transaction_id(), size);
    if (UNLIKELY(!ok)) return NULL;

    uint32_t service_be = htobe32(service_record);
    ok = bte_buffer_writer_write(&writer, &service_be, sizeof(service_be));
    if (UNLIKELY(!ok)) goto error;

    uint16_t max_count_be = htobe16(max_count);
    ok = bte_buffer_writer_write(&writer, &max_count_be, sizeof(max_count_be));
    if (UNLIKELY(!ok)) goto error;

    ok = bte_buffer_writer_write(&writer, id_list, id_list_size);
    if (UNLIKELY(!ok)) goto error;

    sdp->continuation_offset = 4 + 2 + id_list_size;
    ok = write_cont_state(&writer, NULL);
    if (UNLIKELY(!ok)) goto error;

    return bte_buffer_writer_end(&writer);

error:
    bte_buffer_unref(writer.buffer);
    return NULL;
}

bool bte_sdp_service_attr_req(BteSdpClient *sdp, uint32_t service_record,
                              uint16_t max_count, const uint8_t *id_list,
                              BteSdpServiceAttrCb cb, void *userdata)
{
    if (sdp->last_req) return false;

    BteBuffer *buffer =
        service_attr_req_packet(sdp, service_record, max_count, id_list);
    if (UNLIKELY(!buffer)) return false;

    sdp->last_req = bte_buffer_ref(buffer);
    int rc = bte_l2cap_send_message(sdp->l2cap, buffer);
    if (UNLIKELY(rc < 0)) {
        bte_buffer_unref(sdp->last_req);
        sdp->last_req = NULL;
        return false;
    }

    sdp->req_data.service_attr.cb = cb;
    sdp->req_data.service_attr.userdata = userdata;
    sdp->req_data.service_attr.attr_list_de = NULL;
    sdp->req_data.service_attr.written = 0;
    return true;
}

static BteBuffer *service_search_attr_req_packet(
    BteSdpClient *sdp, const uint8_t *pattern, uint16_t max_count,
    const uint8_t *id_list)
{
    uint32_t pattern_size = bte_sdp_de_get_total_size(pattern);
    uint32_t id_list_size = bte_sdp_de_get_total_size(id_list);
    uint16_t size = pattern_size + 2 + id_list_size + 1;

    BteBufferWriter writer;
    bool ok = create_pdu(sdp->l2cap, &writer, PDU_ID_SERVICE_SEARCH_ATTR_REQ,
                         next_transaction_id(), size);
    if (UNLIKELY(!ok)) return NULL;

    ok = bte_buffer_writer_write(&writer, pattern, pattern_size);
    if (UNLIKELY(!ok)) goto error;

    uint16_t max_count_be = htobe16(max_count);
    ok = bte_buffer_writer_write(&writer, &max_count_be, sizeof(max_count_be));
    if (UNLIKELY(!ok)) goto error;

    ok = bte_buffer_writer_write(&writer, id_list, id_list_size);
    if (UNLIKELY(!ok)) goto error;

    sdp->continuation_offset = pattern_size + 2 + id_list_size;
    ok = write_cont_state(&writer, NULL);
    if (UNLIKELY(!ok)) goto error;

    return bte_buffer_writer_end(&writer);

error:
    bte_buffer_unref(writer.buffer);
    return NULL;
}

bool bte_sdp_service_search_attr_req(
    BteSdpClient *sdp, const uint8_t *pattern, uint16_t max_count,
    const uint8_t *id_list, BteSdpServiceSearchAttrCb cb, void *userdata)
{
    if (sdp->last_req) return false;

    BteBuffer *buffer =
        service_search_attr_req_packet(sdp, pattern, max_count, id_list);
    if (UNLIKELY(!buffer)) return false;

    sdp->last_req = bte_buffer_ref(buffer);
    int rc = bte_l2cap_send_message(sdp->l2cap, buffer);
    if (UNLIKELY(rc < 0)) {
        bte_buffer_unref(sdp->last_req);
        sdp->last_req = NULL;
        return false;
    }

    sdp->req_data.service_attr.cb = cb;
    sdp->req_data.service_attr.userdata = userdata;
    sdp->req_data.service_attr.attr_list_de = NULL;
    sdp->req_data.service_attr.written = 0;
    return true;
}

void bte_sdp_reset()
{
    s_next_transaction_id = 0;
}
