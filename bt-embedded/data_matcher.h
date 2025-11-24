#ifndef BTE_DATA_MATCHER_H
#define BTE_DATA_MATCHER_H

#include "utils.h"
#include "types.h"

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BUILDING_BT_EMBEDDED
#error "This is not a public header!"
#endif

#define BTE_DATA_MATCHER_MAX_LEN 16

/* The BteDataMatcher class is used to match incoming packets against a pattern
 * of bytes, so that the appropriate callbacks can be invoked. Things that we
 * can compare include command opcodes, Bt addresses, connection handles.
 */
typedef struct bte_data_matcher_t {
    uint8_t num_rules;
    /* An array of variable-sized elements, each following this pattern:
     * - 1 byte: offset into the compared data (for matching)
     * - 1 byte: length of the following data
     * - 1..length: data
     */
    uint8_t bytes[BTE_DATA_MATCHER_MAX_LEN];
} BTE_PACKED BteDataMatcher;

static inline void bte_data_matcher_init(BteDataMatcher *matcher)
{
    matcher->num_rules = 0;
}

static inline bool bte_data_matcher_is_empty(const BteDataMatcher *matcher)
{
    return matcher->num_rules == 0;
}

static inline void bte_data_matcher_copy(BteDataMatcher *dest,
                                         const BteDataMatcher *src)
{
    memcpy(dest, src, sizeof(BteDataMatcher));
}

static inline bool bte_data_matcher_add_rule(BteDataMatcher *matcher,
                                             const void *data, uint8_t len,
                                             uint8_t offset)
{
    uint8_t *ptr = matcher->bytes;
    for (int i = 0; i < matcher->num_rules; i++) {
        uint8_t len = ptr[1];
        ptr += 2 + len;
    }
    if (UNLIKELY((ptr - matcher->bytes) + 2 + len > BTE_DATA_MATCHER_MAX_LEN))
        return false;

    ptr[0] = offset;
    ptr[1] = len;
    memcpy(ptr + 2, data, len);
    matcher->num_rules++;
    return true;
}

static inline bool bte_data_matcher_compare(const BteDataMatcher *matcher,
                                            const void *data, uint8_t data_len)
{
    const uint8_t *ptr = matcher->bytes;
    for (int i = 0; i < matcher->num_rules; i++) {
        uint8_t offset = ptr[0];
        uint8_t len = ptr[1];
        if (UNLIKELY(offset + len > data_len)) return false;

        if (memcmp(ptr + 2, (const uint8_t *)data + offset, len) != 0)
            return false;
        ptr += 2 + len;
    }
    return true;
}

static inline bool bte_data_matcher_is_same(const BteDataMatcher *a,
                                            const BteDataMatcher *b)
{
    if (a->num_rules != b->num_rules) return false;

    const uint8_t *ptr_a = a->bytes;
    const uint8_t *ptr_b = b->bytes;
    for (int i = 0; i < a->num_rules; i++) {
        if (ptr_a[0] != ptr_b[0] || ptr_a[1] != ptr_b[1]) return false;

        uint8_t len = ptr_a[1];
        if (memcmp(ptr_a + 2, ptr_b + 2, len) != 0) return false;
        ptr_a += 2 + len;
        ptr_b += 2 + len;
    }
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* BTE_DATA_MATCHER_H */
