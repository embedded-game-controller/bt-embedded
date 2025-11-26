#ifndef BTE_TEST_TYPE_UTILS_H
#define BTE_TEST_TYPE_UTILS_H

#include "bt-embedded/hci.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <ranges>
#include <vector>

inline std::ostream &operator<<(std::ostream &os, const BteBdAddr &a)
{
    for (size_t i = 0; i < sizeof(a); i++) {
        os << std::hex << std::setw(2) << std::setfill('0') << int(a.bytes[i]);
        if (i < sizeof(a) - 1) os << ':';
    }
    return os;
}

inline std::ostream &operator<<(std::ostream &os, const BteLinkKey &a)
{
    for (size_t i = 0; i < sizeof(a); i++) {
        os << std::hex << std::setw(2) << std::setfill('0') << int(a.bytes[i]);
        if (i < sizeof(a) - 1) os << '-';
    }
    return os;
}

inline bool operator==(const BteClassOfDevice &a, const BteClassOfDevice &b)
{
    return memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

inline bool operator==(const BteHciReply &a, const BteHciReply &b)
{
    return a.status == b.status;
}

inline bool operator==(const BteHciCreateConnectionReply &a,
                       const BteHciCreateConnectionReply &b)
{
    return a.status == b.status && a.link_type == b.link_type &&
        a.conn_handle == b.conn_handle && a.address == b.address &&
        a.encryption_mode == b.encryption_mode;
}

inline bool operator==(const BteHciLinkKeyReqReply &a,
                       const BteHciLinkKeyReqReply &b)
{
    return a.status == b.status && a.address == b.address;
}

inline bool operator==(const BteHciPinCodeReqReply &a,
                       const BteHciPinCodeReqReply &b)
{
    return a.status == b.status && a.address == b.address;
}

inline bool operator==(const BteHciReadPinTypeReply &a,
                       const BteHciReadPinTypeReply &b)
{
    return a.status == b.status && a.pin_type == b.pin_type;
}

inline bool operator==(const BteHciReadLinkPolicySettingsReply &a,
                       const BteHciReadLinkPolicySettingsReply &b)
{
    return a.status == b.status && a.conn_handle == b.conn_handle &&
        a.settings == b.settings;
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

inline bool operator==(const BteHciWriteStoredLinkKeyReply &a,
                       const BteHciWriteStoredLinkKeyReply &b)
{
    return a.status == b.status && a.num_keys == b.num_keys;
}

inline bool operator==(const BteHciDeleteStoredLinkKeyReply &a,
                       const BteHciDeleteStoredLinkKeyReply &b)
{
    return a.status == b.status && a.num_keys == b.num_keys;
}

inline bool operator==(const BteHciReadPageTimeoutReply &a,
                       const BteHciReadPageTimeoutReply &b)
{
    return a.status == b.status && a.page_timeout == b.page_timeout;
}

inline bool operator==(const BteHciReadLocalNameReply &a,
                       const BteHciReadLocalNameReply &b)
{
    return a.status == b.status &&
        strncmp(a.name, b.name, sizeof(a.name)) == 0;
}

inline bool operator==(const BteHciReadScanEnableReply &a,
                       const BteHciReadScanEnableReply &b)
{
    return a.status == b.status && a.scan_enable == b.scan_enable;
}

inline bool operator==(const BteHciReadAuthEnableReply &a,
                       const BteHciReadAuthEnableReply &b)
{
    return a.status == b.status && a.auth_enable == b.auth_enable;
}

inline bool operator==(const BteHciReadClassOfDeviceReply &a,
                       const BteHciReadClassOfDeviceReply &b)
{
    return a.status == b.status && a.cod == b.cod;
}

inline bool operator==(const BteHciReadAutoFlushTimeoutReply &a,
                       const BteHciReadAutoFlushTimeoutReply &b)
{
    return a.status == b.status && a.conn_handle == b.conn_handle &&
        a.flush_timeout == b.flush_timeout;
}

inline bool operator==(const BteHciReadLinkSvTimeoutReply &a,
                       const BteHciReadLinkSvTimeoutReply &b)
{
    return a.status == b.status && a.conn_handle == b.conn_handle &&
        a.sv_timeout == b.sv_timeout;
}

inline bool operator==(const BteHciReadInquiryScanTypeReply &a,
                       const BteHciReadInquiryScanTypeReply &b)
{
    return a.status == b.status && a.inquiry_scan_type == b.inquiry_scan_type;
}

inline bool operator==(const BteHciReadInquiryModeReply &a,
                       const BteHciReadInquiryModeReply &b)
{
    return a.status == b.status && a.inquiry_mode == b.inquiry_mode;
}

inline bool operator==(const BteHciReadPageScanTypeReply &a,
                       const BteHciReadPageScanTypeReply &b)
{
    return a.status == b.status && a.page_scan_type == b.page_scan_type;
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

struct ReadCurrentIacLapReply {
    ReadCurrentIacLapReply(const Bte::Client::Hci::ReadCurrentIacLapReply &r):
        status(r.status) {
        laps.assign(r.laps.begin(), r.laps.end());
    }
    ReadCurrentIacLapReply(uint8_t status, const std::vector<BteLap> &laps):
        status(status), laps(laps) {}
    ReadCurrentIacLapReply(int zero): status(0) {}

    uint8_t status;
    std::vector<BteLap> laps;
};

inline bool operator==(const ReadCurrentIacLapReply &a,
                       const ReadCurrentIacLapReply &b)
{
    return a.status == b.status && a.laps == b.laps;
}

inline std::ostream &operator<<(std::ostream &os,
                                const ReadCurrentIacLapReply &r)
{
    os << "(status " << r.status << ")[";
    for (const auto &e : r.laps)
        os << std::hex << std::setw(6) << setfill('0') << e << ", ";
    os << "]";
    return os;
}

} /* namespace StoredTypes */

#endif /* BTE_TEST_TYPE_UTILS_H */
