#ifndef BTE_TEST_TYPE_UTILS_H
#define BTE_TEST_TYPE_UTILS_H

#include "bt-embedded/hci.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <ranges>

inline bool operator==(const BteBdAddr &a, const BteBdAddr &b)
{
    return memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

inline std::ostream &operator<<(std::ostream &os, const BteBdAddr &a)
{
    for (size_t i = 0; i < sizeof(a); i++) {
        os << std::hex << std::setw(2) << setfill('0') << int(a.bytes[i]);
        if (i < sizeof(a) - 1) os << ':';
    }
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const BteLinkKey &a)
{
    for (size_t i = 0; i < sizeof(a); i++) {
        os << std::hex << std::setw(2) << setfill('0') << int(a.bytes[i]);
        if (i < sizeof(a) - 1) os << '-';
    }
    return os;
}

inline bool operator==(const BteHciReply &a, const BteHciReply &b)
{
    return a.status == b.status;
}

inline bool operator==(const BteHciLinkKeyReqReply &a,
                       const BteHciLinkKeyReqReply &b)
{
    return a.status == b.status && a.address == b.address;
}

inline bool operator==(const BteHciStoredLinkKey &a,
                       const BteHciStoredLinkKey &b)
{
    return memcmp(&a.address, &b.address, sizeof(a.address)) == 0 &&
        memcmp(&a.key, &b.key, sizeof(a.key)) == 0;
}

namespace Bte {
inline bool operator==(const Client::Hci::ReadStoredLinkKeyReply &a,
                       const Client::Hci::ReadStoredLinkKeyReply &b)
{
    return a.status == b.status && a.max_keys == b.max_keys &&
        std::ranges::equal(a.stored_keys, b.stored_keys);
}
} /* namespace Bte */

inline bool operator==(const BteHciReadLocalNameReply &a,
                       const BteHciReadLocalNameReply &b)
{
    return a.status == b.status &&
        strncmp(a.name, b.name, sizeof(a.name)) == 0;
}

inline bool operator==(const BteHciReadLocalVersionReply &a,
                       const BteHciReadLocalVersionReply &b)
{
    return a.status == b.status &&
        a.hci_version == b.hci_version &&
        a.hci_revision == b.hci_revision &&
        a.lmp_version == b.lmp_version &&
        a.manufacturer == b.manufacturer &&
        a.lmp_subversion == b.lmp_subversion;
}

inline bool operator==(const BteHciReadLocalFeaturesReply &a,
                       const BteHciReadLocalFeaturesReply &b)
{
    return a.status == b.status && a.features == b.features;
}

inline bool operator==(const BteHciReadBufferSizeReply &a,
                       const BteHciReadBufferSizeReply &b)
{
    return a.status == b.status &&
        a.sco_mtu == b.sco_mtu &&
        a.acl_mtu == b.acl_mtu &&
        a.sco_max_packets == b.sco_max_packets &&
        a.acl_max_packets == b.acl_max_packets;
}

inline bool operator==(const BteHciReadBdAddrReply &a,
                       const BteHciReadBdAddrReply &b)
{
    return a.status == b.status &&
        memcmp(&a.address, &b.address, sizeof(a.address));
}

struct StoredInquiryReply {
    using BteType = BteHciInquiryReply;
    StoredInquiryReply() = default;
    StoredInquiryReply(const BteHciInquiryReply &r) {
        status = r.status;
        num_responses = r.num_responses;
        for (int i = 0; i < num_responses; i++) {
            responses.push_back(r.responses[i]);
        }
    }
    uint8_t status = 0xff;
    uint8_t num_responses = 0;
    std::vector<BteHciInquiryResponse> responses;
};

inline bool operator==(const BteHciInquiryReply &a, const BteHciInquiryReply &b)
{
    return a.status == b.status &&
        a.num_responses == b.num_responses &&
        memcmp(a.responses, b.responses,
               sizeof(BteHciInquiryResponse) * a.num_responses) == 0;
}

namespace StoredTypes {

struct ReadStoredLinkKeyReply {
    ReadStoredLinkKeyReply(const Bte::Client::Hci::ReadStoredLinkKeyReply &r):
        status(r.status),
        max_keys(r.max_keys)
    {
        stored_keys.assign(r.stored_keys.begin(), r.stored_keys.end());
    }
    ReadStoredLinkKeyReply(uint8_t status, uint16_t max_keys,
                           const std::vector<BteHciStoredLinkKey> &stored_keys):
        status(status), max_keys(max_keys), stored_keys(stored_keys) {}

    uint8_t status;
    uint16_t max_keys;
    std::vector<BteHciStoredLinkKey> stored_keys;
};

inline bool operator==(const ReadStoredLinkKeyReply &a,
                       const ReadStoredLinkKeyReply &b)
{
    return a.status == b.status && a.max_keys == b.max_keys &&
        a.stored_keys == b.stored_keys;
}

inline std::ostream &operator<<(std::ostream &os,
                                const ReadStoredLinkKeyReply &r)
{
    os << "(status " << r.status << ", max " << r.max_keys << ")[";
    for (const auto &e : r.stored_keys)
        os << '{' << e.address << ", " << e.key << "}\n";
    os << "]";
    return os;
}

} /* namespace StoredTypes */

#endif /* BTE_TEST_TYPE_UTILS_H */
