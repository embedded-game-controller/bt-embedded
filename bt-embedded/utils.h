#ifndef BTE_UTILS_H
#define BTE_UTILS_H

#include <assert.h>
#include <string.h>
#include <sys/endian.h>

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

#endif /* BTE_UTILS_H */
