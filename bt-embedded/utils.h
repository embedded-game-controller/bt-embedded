#ifndef BTE_UTILS_H
#define BTE_UTILS_H

#include <assert.h>
#ifndef __wii__
#  include <endian.h>
#else
#  include <sys/endian.h>
#endif
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BUILDING_BT_EMBEDDED
#error "This is not a public header!"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define BIT(nr)        (1ull << (nr))
#define MIN2(x, y)     (((x) < (y)) ? (x) : (y))
#define MAX2(x, y)     (((x) > (y)) ? (x) : (y))
#define ROUNDUP32(x)   (((u32)(x) + 0x1f) & ~0x1f)
#define ROUNDDOWN32(x) (((u32)(x) - 0x1f) & ~0x1f)

#define UNUSED(x)                 (void)(x)
#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

#ifndef NORETURN
#define NORETURN __attribute__((noreturn))
#endif

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define read_le16(ptr) le16toh(*(uint16_t *)(ptr))
#define read_le32(ptr) le32toh(*(uint32_t *)(ptr))
#define read_le64(ptr) le64toh(*(uint64_t *)(ptr))

#define write_le16(n, ptr) *(uint16_t *)(ptr) = htole16(n)
#define write_le32(n, ptr) *(uint32_t *)(ptr) = htole32(n)
#define write_le64(n, ptr) *(uint64_t *)(ptr) = htole64(n)

static inline void ensure_array_size(void **ptr, size_t elem_size,
                                     int elem_per_block,
                                     int num_elem_curr, int num_elem_added)
{
    int allocated_blocks =
        (num_elem_curr + elem_per_block - 1) / elem_per_block;
    int needed_blocks =
        (num_elem_curr + num_elem_added + elem_per_block - 1) / elem_per_block;
    if (needed_blocks > allocated_blocks) {
        /* Allocate more blocks */
        int n = needed_blocks * elem_per_block;
        *ptr = realloc(*ptr, n * elem_size);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BTE_UTILS_H */
