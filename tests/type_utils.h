#ifndef BTE_TEST_TYPE_UTILS_H
#define BTE_TEST_TYPE_UTILS_H

#include "bt-embedded/hci.h"

inline bool operator==(const BteBdAddr &a, const BteBdAddr &b)
{
    return memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

inline bool operator==(const BteHciReply &a, const BteHciReply &b)
{
    return a.status == b.status;
}

struct StoredInquiryReply {
    StoredInquiryReply(const BteHciInquiryReply &r) {
        status = r.status;
        num_responses = r.num_responses;
        for (int i = 0; i < num_responses; i++) {
            responses.push_back(r.responses[i]);
        }
    }
    uint8_t status;
    uint8_t num_responses;
    std::vector<BteHciInquiryResponse> responses;
};

inline bool operator==(const BteHciInquiryReply &a, const BteHciInquiryReply &b)
{
    return a.status == b.status &&
        a.num_responses == b.num_responses &&
        memcmp(a.responses, b.responses,
               sizeof(BteHciInquiryResponse) * a.num_responses) == 0;
}

#endif /* BTE_TEST_TYPE_UTILS_H */
