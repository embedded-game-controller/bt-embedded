#ifndef BTE_TESTS_BTE_CPP_H
#define BTE_TESTS_BTE_CPP_H

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"

#include <any>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>

inline bool operator==(const BteBdAddr &a, const BteBdAddr &b)
{
    return memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

template <> struct std::hash<BteBdAddr> {
    size_t operator()(const BteBdAddr &a) const noexcept {
        return std::hash<std::string>()(
            std::string(reinterpret_cast<const char*>(a.bytes),
                        sizeof(a.bytes)));
    }
};

/* C++ wrapper for the bt-embedded API. Used for testing, but we might make it
 * part of bt-embedded, if someone needs it. */
namespace Bte {

class Buffer: public std::vector<uint8_t> {
public:
    using std::vector<uint8_t>::vector;
    Buffer(const std::vector<uint8_t> &data):
        std::vector<uint8_t>(data) {}
    template <typename T>
    Buffer(const T &var):
        std::vector<uint8_t>(sizeof(var)) {
        memcpy(data(), &var, sizeof(var));
    }
    Buffer(BteBuffer *buffer);

    BteBuffer *toBuffer(uint16_t max_packet_size = 0) const;

    Buffer operator+(const Buffer &other) const {
        Buffer ret(static_cast<std::vector<uint8_t>>(*this));
        ret.insert(ret.end(), other.begin(), other.end());
        return ret;
    }
    Buffer &operator+=(const Buffer &other) {
        insert(end(), other.begin(), other.end());
        return *this;
    }
};

class Client {
public:
    Client(): m_client(bte_client_new()), m_hci(bte_hci_get(m_client)) {
        init();
    }
    Client(const Client &other):
        m_client(bte_client_ref(other.m_client)),
        m_hci(bte_hci_get(m_client)) {
        init();
    }
    ~Client() { bte_client_unref(m_client); }

    struct Hci {
    private:
        using Tag = int;
#define TAG __LINE__
        template <typename CbData, Tag t>
        static CbData cbData(Hci &hci) {
            return std::any_cast<CbData>(hci.m_callbacks[t]);
        }

        template <typename CbData, Tag t>
        static CbData cbData(void *userdata) {
            return cbData<CbData, t>(static_cast<Client*>(userdata)->m_hci);
        }

        template <typename ReplyType, typename CbType>
        struct CallbackHelper {
            static void commandCb(BteHci *, ReplyType *reply, void *cb_data) {
                if constexpr (std::is_same_v<CbType,std::function<void(ReplyType &)>>) {
                    cbData<CbType>(cb_data, &CallbackHelper::commandCb)(*reply);
                } else {
                    cbData<CbType>(cb_data, &CallbackHelper::commandCb)(reply);
                }
            }
        };

        template <Tag t, typename ReplyType, typename CbType>
        struct TaggedCallbackHelper {
            static void commandCb(BteHci *, ReplyType *reply, void *cb_data) {
                if constexpr (std::is_same_v<CbType,std::function<void(ReplyType &)>>) {
                    cbData<CbType, t>(cb_data)(*reply);
                } else {
                    cbData<CbType, t>(cb_data)(reply);
                }
            }
        };

        template <Tag t, typename ReplyType, typename CbType>
        auto wrap(const CbType &cb) {
            auto commandCb = &TaggedCallbackHelper<t, ReplyType, CbType>::commandCb;
            m_callbacks[t] = cb;
            return commandCb;
        }

        /* Version for disambiguating the callbacks where CbType is the same
         * (most functions use DoneCb as callback type, so we need a way to
         * tell them apart). */
        template <Tag t, typename CbType>
        auto wrap(const CbType &cb) {
            using ReplyType = typename extract_argument<CbType>::type;
            auto commandCb = &TaggedCallbackHelper<t, ReplyType, CbType>::commandCb;
            m_callbacks[t] = cb;
            return commandCb;
        }

        /* Simplified version of the wrap() function with automatic deduction
         * of the type from the argument of the CbType callback. */
        template <typename> struct extract_argument;

        template <typename R, typename A>
        struct extract_argument<std::function<R(A)>>
        {
            using type = std::remove_reference_t<A>;
        };

    public:
        using InitializedCb = std::function<void(bool)>;
        void onInitialized(const InitializedCb &cb) {
            m_initializedCb = cb;
            bte_hci_on_initialized(m_hci, &Hci::Callbacks::initialized);
        }

        using DoneCb = std::function<void(const BteHciReply &)>;
        void nop(const DoneCb &cb) {
            bte_hci_nop(m_hci, wrap<TAG>(cb));
        }

        using InquiryCb = std::function<void(const BteHciInquiryReply &)>;
        void inquiry(BteLap lap, uint8_t len, uint8_t max_resp,
                     const DoneCb &statusCb, const InquiryCb &cb) {
            m_inquiryCb = {statusCb, cb};
            bte_hci_inquiry(m_hci, lap, len, max_resp,
                            &Hci::Callbacks::inquiryStatus,
                            &Hci::Callbacks::inquiry);
        }

        void inquiryCancel(const DoneCb &cb) {
            bte_hci_inquiry_cancel(m_hci, wrap<TAG>(cb));
        }

        void periodicInquiry(uint16_t min_period, uint16_t max_period,
                             BteLap lap, uint8_t len, uint8_t max_resp,
                             const DoneCb &statusCb, const InquiryCb &cb) {
            m_inquiryCb = {statusCb, cb};
            bte_hci_periodic_inquiry(m_hci, min_period, max_period,
                                     lap, len, max_resp,
                                     &Hci::Callbacks::inquiryStatus,
                                     &Hci::Callbacks::inquiry);
        }

        void exitPeriodicInquiry(const DoneCb &cb) {
            bte_hci_exit_periodic_inquiry(m_hci, wrap<TAG>(cb));
        }

        using CreateConnectionCb =
            std::function<void(const BteHciCreateConnectionReply &)>;
        void createConnection(const BteBdAddr &address,
                              BtePacketType packet_type,
                              uint8_t page_scan_rep_mode,
                              uint16_t clock_offset,
                              bool allow_role_switch,
                              const DoneCb &statusCb,
                              const CreateConnectionCb &cb) {
            m_createConnectionCallbacks[address] = cb;
            bte_hci_create_connection(m_hci, &address, packet_type,
                                      page_scan_rep_mode, clock_offset,
                                      allow_role_switch,
                                      wrap<TAG>(statusCb),
                                      &Hci::Callbacks::createConnection);
        }

        void createConnectionCancel(const BteBdAddr &address,
                                    const DoneCb &cb) {
            bte_hci_create_connection_cancel(m_hci, &address, wrap<TAG>(cb));
        }

        using AcceptConnectionCb = CreateConnectionCb;
        void acceptConnection(const BteBdAddr &address, uint8_t role,
                              const DoneCb &statusCb,
                              const AcceptConnectionCb &cb) {
            m_createConnectionCallbacks[address] = cb;
            bte_hci_accept_connection(m_hci, &address, role,
                                      wrap<TAG>(statusCb),
                                      &Hci::Callbacks::createConnection);
        }

        using RejectConnectionCb = CreateConnectionCb;
        void rejectConnection(const BteBdAddr &address, uint8_t reason,
                              const DoneCb &statusCb,
                              const RejectConnectionCb &cb) {
            m_createConnectionCallbacks[address] = cb;
            bte_hci_reject_connection(m_hci, &address, reason,
                                      wrap<TAG>(statusCb),
                                      &Hci::Callbacks::createConnection);
        }

        using ConnectionRequestCb =
            std::function<bool(const BteBdAddr &address,
                               const BteClassOfDevice &cod,
                               uint8_t link_type)>;
        void onConnectionRequest(const ConnectionRequestCb &cb) {
            m_connectionRequestCb = cb;
            bte_hci_on_connection_request(
                m_hci, &Hci::Callbacks::connectionRequest);
        }

        using LinkKeyRequestCb = std::function<bool(const BteBdAddr &address)>;
        void onLinkKeyRequest(const LinkKeyRequestCb &cb) {
            m_linkKeyRequestCb = cb;
            bte_hci_on_link_key_request(m_hci, &Hci::Callbacks::linkKeyRequest);
        }

        using LinkKeyReqReplyCb =
            std::function<void(const BteHciLinkKeyReqReply &)>;
        void linkKeyReqReply(const BteBdAddr &address, const BteLinkKey &key,
                             const LinkKeyReqReplyCb &cb) {
            bte_hci_link_key_req_reply(m_hci, &address, &key, wrap<TAG>(cb));
        }

        void linkKeyReqNegReply(const BteBdAddr &address,
                                const LinkKeyReqReplyCb &cb) {
            bte_hci_link_key_req_neg_reply(m_hci, &address, wrap<TAG>(cb));
        }

        using PinCodeRequestCb = std::function<bool(const BteBdAddr &address)>;
        void onPinCodeRequest(const PinCodeRequestCb &cb) {
            m_pinCodeRequestCb = cb;
            bte_hci_on_pin_code_request(m_hci, &Hci::Callbacks::pinCodeRequest);
        }

        using PinCodeReqReplyCb =
            std::function<void(const BteHciPinCodeReqReply &)>;
        using PinCode = std::span<const uint8_t>;
        void pinCodeReqReply(const BteBdAddr &address, const PinCode &pin,
                             const PinCodeReqReplyCb &cb) {
            bte_hci_pin_code_req_reply(m_hci, &address, pin.data(), pin.size(),
                                       wrap<TAG>(cb));
        }

        void pinCodeReqNegReply(const BteBdAddr &address,
                                const PinCodeReqReplyCb &cb) {
            bte_hci_pin_code_req_neg_reply(m_hci, &address, wrap<TAG>(cb));
        }

        using AuthRequestedCb =
            std::function<void(const BteHciAuthRequestedReply &)>;
        void authRequested(BteHciConnHandle conn_handle,
                           const DoneCb &statusCb,
                           const AuthRequestedCb &cb) {
            m_authRequestedCallbacks[conn_handle] = cb;
            bte_hci_auth_requested(m_hci, conn_handle,
                                   wrap<TAG>(statusCb),
                                   &Hci::Callbacks::authRequested);
        }

        using ReadRemoteNameCb =
            std::function<void(const BteHciReadRemoteNameReply &)>;
        void readRemoteName(const BteBdAddr &address,
                            uint8_t page_scan_rep_mode,
                            uint16_t clock_offset,
                            const DoneCb &statusCb,
                            const ReadRemoteNameCb &cb) {
            m_readRemoteNameCallbacks[address] = cb;
            bte_hci_read_remote_name(m_hci, &address, page_scan_rep_mode,
                                     clock_offset, wrap<TAG>(statusCb),
                                     &Hci::Callbacks::readRemoteName);
        }

        using ReadRemoteFeaturesCb =
            std::function<void(const BteHciReadRemoteFeaturesReply &)>;
        void readRemoteFeatures(BteHciConnHandle conn_handle,
                                const DoneCb &statusCb,
                                const ReadRemoteFeaturesCb &cb) {
            m_readRemoteFeaturesCallbacks[conn_handle] = cb;
            bte_hci_read_remote_features(m_hci, conn_handle,
                                         wrap<TAG>(statusCb),
                                         &Hci::Callbacks::readRemoteFeatures);
        }

        using ReadRemoteVersionInfoCb =
            std::function<void(const BteHciReadRemoteVersionInfoReply &)>;
        void readRemoteVersionInfo(BteHciConnHandle conn_handle,
                                   const DoneCb &statusCb,
                                   const ReadRemoteVersionInfoCb &cb) {
            m_readRemoteVersionInfoCallbacks[conn_handle] = cb;
            bte_hci_read_remote_version_info(
                m_hci, conn_handle, wrap<TAG>(statusCb),
                &Hci::Callbacks::readRemoteVersionInfo);
        }

        using ReadClockOffsetCb =
            std::function<void(const BteHciReadClockOffsetReply &)>;
        void readClockOffset(BteHciConnHandle conn_handle,
                             const DoneCb &statusCb,
                             const ReadClockOffsetCb &cb) {
            m_readClockOffsetCallbacks[conn_handle] = cb;
            bte_hci_read_clock_offset(m_hci, conn_handle, wrap<TAG>(statusCb),
                                      &Hci::Callbacks::readClockOffset);
        }

        void setSniffMode(BteHciConnHandle conn_handle,
                          uint16_t min_interval, uint16_t max_interval,
                          uint16_t attempt_slots, uint16_t timeout,
                          const DoneCb &cb) {
            bte_hci_set_sniff_mode(
                m_hci, conn_handle, min_interval, max_interval,
                attempt_slots, timeout, wrap<TAG>(cb));
        }

        using ModeChangeCb =
            std::function<bool(const BteHciModeChangeReply &reply)>;
        void onModeChange(BteHciConnHandle conn_handle,
                          const ModeChangeCb &cb) {
            m_modeChangeCb = cb;
            bte_hci_on_mode_change(m_hci, conn_handle,
                                   cb ? &Hci::Callbacks::modeChange : nullptr);
        }

        void writeLinkPolicySettings(BteHciConnHandle conn_handle,
                                     BteHciLinkPolicySettings settings,
                                     const DoneCb &cb) {
            bte_hci_write_link_policy_settings(
                m_hci, conn_handle, settings, wrap<TAG>(cb));
        }

        using ReadLinkPolicySettingsCb =
            std::function<void(const BteHciReadLinkPolicySettingsReply &)>;
        void readLinkPolicySettings(BteHciConnHandle conn_handle,
                                    const ReadLinkPolicySettingsCb &cb) {
            bte_hci_read_link_policy_settings(m_hci, conn_handle,
                                              wrap<TAG>(cb));
        }

        void setEventMask(BteHciEventMask mask, const DoneCb &cb) {
            bte_hci_set_event_mask(m_hci, mask, wrap<TAG>(cb));
        }

        void reset(const DoneCb &cb) {
            bte_hci_reset(m_hci, wrap<TAG>(cb));
        }

        void writePinType(uint8_t pin_type, const DoneCb &cb) {
            bte_hci_write_pin_type(m_hci, pin_type, wrap<TAG>(cb));
        }

        using ReadPinTypeCb =
            std::function<void(const BteHciReadPinTypeReply &)>;
        void readPinType(const ReadPinTypeCb &cb) {
            bte_hci_read_pin_type(m_hci, wrap<TAG>(cb));
        }

        struct ReadStoredLinkKeyReply {
            uint8_t status;
            uint16_t max_keys;
            std::span<const BteHciStoredLinkKey> stored_keys;
            ReadStoredLinkKeyReply(const BteHciReadStoredLinkKeyReply *r):
                status(r->status), max_keys(r->max_keys),
                stored_keys(r->stored_keys, r->num_keys) {}
        };
        using ReadStoredLinkKeyCb =
            std::function<void(const ReadStoredLinkKeyReply &)>;
        void readStoredLinkKey(const BteBdAddr &address,
                               const ReadStoredLinkKeyCb &cb) {
            bte_hci_read_stored_link_key(m_hci, &address,
                wrap<TAG, const BteHciReadStoredLinkKeyReply>(cb));
        }
        void readStoredLinkKey(const ReadStoredLinkKeyCb &cb) {
            bte_hci_read_stored_link_key(m_hci, nullptr,
                wrap<TAG, const BteHciReadStoredLinkKeyReply>(cb));
        }

        using WriteStoredLinkKeyCb =
            std::function<void(const BteHciWriteStoredLinkKeyReply &)>;
        void writeStoredLinkKey(
            const std::span<const BteHciStoredLinkKey> &keys,
            const WriteStoredLinkKeyCb &cb) {
            bte_hci_write_stored_link_key(m_hci, keys.size(), keys.data(),
                                          wrap<TAG>(cb));
        }

        using DeleteStoredLinkKeyCb =
            std::function<void(const BteHciDeleteStoredLinkKeyReply &)>;
        void deleteStoredLinkKey(const BteBdAddr &address,
                                 const DeleteStoredLinkKeyCb &cb) {
            bte_hci_delete_stored_link_key(m_hci, &address, wrap<TAG>(cb));
        }
        void deleteStoredLinkKey(const DeleteStoredLinkKeyCb &cb) {
            bte_hci_delete_stored_link_key(m_hci, nullptr, wrap<TAG>(cb));
        }

        void writeLocalName(const std::string &name, const DoneCb &cb) {
            bte_hci_write_local_name(m_hci, name.c_str(), wrap<TAG>(cb));
        }

        using ReadLocalNameCb =
            std::function<void(const BteHciReadLocalNameReply &)>;
        void readLocalName(const ReadLocalNameCb &cb) {
            bte_hci_read_local_name(m_hci, wrap<TAG>(cb));
        }

        void writePageTimeout(uint16_t page_timeout, const DoneCb &cb) {
            bte_hci_write_page_timeout(m_hci, page_timeout, wrap<TAG>(cb));
        }

        using ReadPageTimeoutCb =
            std::function<void(const BteHciReadPageTimeoutReply &)>;
        void readPageTimeout(const ReadPageTimeoutCb &cb) {
            bte_hci_read_page_timeout(m_hci, wrap<TAG>(cb));
        }

        void writeScanEnable(uint8_t scan_enable, const DoneCb &cb) {
            bte_hci_write_scan_enable(m_hci, scan_enable, wrap<TAG>(cb));
        }

        using ReadScanEnableCb =
            std::function<void(const BteHciReadScanEnableReply &)>;
        void readScanEnable(const ReadScanEnableCb &cb) {
            bte_hci_read_scan_enable(m_hci, wrap<TAG>(cb));
        }

        void writeAuthEnable(uint8_t auth_enable, const DoneCb &cb) {
            bte_hci_write_auth_enable(m_hci, auth_enable, wrap<TAG>(cb));
        }

        using ReadAuthEnableCb =
            std::function<void(const BteHciReadAuthEnableReply &)>;
        void readAuthEnable(const ReadAuthEnableCb &cb) {
            bte_hci_read_auth_enable(m_hci, wrap<TAG>(cb));
        }

        void writeClassOfDevice(const BteClassOfDevice &cod, const DoneCb &cb) {
            bte_hci_write_class_of_device(m_hci, &cod, wrap<TAG>(cb));
        }

        using ReadClassOfDeviceCb =
            std::function<void(const BteHciReadClassOfDeviceReply &)>;
        void readClassOfDevice(const ReadClassOfDeviceCb &cb) {
            bte_hci_read_class_of_device(m_hci, wrap<TAG>(cb));
        }

        void writeAutoFlushTimeout(BteHciConnHandle conn_handle,
                                   uint8_t timeout, const DoneCb &cb) {
            bte_hci_write_auto_flush_timeout(
                m_hci, conn_handle, timeout, wrap<TAG>(cb));
        }

        using ReadAutoFlushTimeoutCb =
            std::function<void(const BteHciReadAutoFlushTimeoutReply &)>;
        void readAutoFlushTimeout(BteHciConnHandle conn_handle,
                                  const ReadAutoFlushTimeoutCb &cb) {
            bte_hci_read_auto_flush_timeout(m_hci, conn_handle, wrap<TAG>(cb));
        }

        void setCtrlToHostFlowControl(BteHciConnHandle conn_handle,
                                      uint8_t enable, const DoneCb &cb) {
            bte_hci_set_ctrl_to_host_flow_control(
                m_hci, enable, wrap<TAG>(cb));
        }

        void setHostBufferSize(BteHciConnHandle conn_handle,
                               uint16_t acl_packet_len, uint16_t acl_packets,
                               uint8_t sync_packet_len, uint16_t sync_packets,
                               const DoneCb &cb) {
            bte_hci_set_host_buffer_size(m_hci, acl_packet_len, acl_packets,
                                         sync_packet_len, sync_packets,
                                         wrap<TAG>(cb));
        }

        void writeCurrentIacLap(const std::span<const BteLap> &laps,
                                const DoneCb &cb) {
            bte_hci_write_current_iac_lap(
                m_hci, uint8_t(laps.size()), laps.data(), wrap<TAG>(cb));
        }

        struct ReadCurrentIacLapReply {
            uint8_t status;
            std::span<const BteLap> laps;
            ReadCurrentIacLapReply(const BteHciReadCurrentIacLapReply *r):
                status(r->status), laps(r->laps, r->num_laps) {}
        };
        using ReadCurrentIacLapCb =
            std::function<void(const ReadCurrentIacLapReply &)>;
        void readCurrentIacLap(BteHciConnHandle conn_handle,
                               const ReadCurrentIacLapCb &cb) {
            bte_hci_read_current_iac_lap(
                m_hci, wrap<TAG, const BteHciReadCurrentIacLapReply>(cb));
        }

        void writeLinkSvTimeout(BteHciConnHandle conn_handle,
                                uint8_t timeout, const DoneCb &cb) {
            bte_hci_write_link_sv_timeout(
                m_hci, conn_handle, timeout, wrap<TAG>(cb));
        }

        using ReadLinkSvTimeoutCb =
            std::function<void(const BteHciReadLinkSvTimeoutReply &)>;
        void readLinkSvTimeout(BteHciConnHandle conn_handle,
                               const ReadLinkSvTimeoutCb &cb) {
            bte_hci_read_link_sv_timeout(m_hci, conn_handle, wrap<TAG>(cb));
        }

        void writeInquiryScanType(uint8_t inquiry_scan_type,
                                  const DoneCb &cb) {
            bte_hci_write_inquiry_scan_type(
                m_hci, inquiry_scan_type, wrap<TAG>(cb));
        }

        using ReadInquiryScanTypeCb =
            std::function<void(const BteHciReadInquiryScanTypeReply &)>;
        void readInquiryScanType(const ReadInquiryScanTypeCb &cb) {
            bte_hci_read_inquiry_scan_type(m_hci, wrap<TAG>(cb));
        }

        void writeInquiryMode(uint8_t inquiry_mode, const DoneCb &cb) {
            bte_hci_write_inquiry_mode(m_hci, inquiry_mode, wrap<TAG>(cb));
        }

        using ReadInquiryModeCb =
            std::function<void(const BteHciReadInquiryModeReply &)>;
        void readInquiryMode(const ReadInquiryModeCb &cb) {
            bte_hci_read_inquiry_mode(m_hci, wrap<TAG>(cb));
        }

        void writePageScanType(uint8_t page_scan_type, const DoneCb &cb) {
            bte_hci_write_page_scan_type(m_hci, page_scan_type, wrap<TAG>(cb));
        }

        using ReadPageScanTypeCb =
            std::function<void(const BteHciReadPageScanTypeReply &)>;
        void readPageScanType(const ReadPageScanTypeCb &cb) {
            bte_hci_read_page_scan_type(m_hci, wrap<TAG>(cb));
        }

        using ReadLocalVersionCb =
            std::function<void(const BteHciReadLocalVersionReply &)>;
        void readLocalVersion(const ReadLocalVersionCb &cb) {
            bte_hci_read_local_version(m_hci, wrap<TAG>(cb));
        }

        using ReadLocalFeaturesCb =
            std::function<void(const BteHciReadLocalFeaturesReply &)>;
        void readLocalFeatures(const ReadLocalFeaturesCb &cb) {
            bte_hci_read_local_features(m_hci, wrap<TAG>(cb));
        }

        using ReadBufferSizeCb =
            std::function<void(const BteHciReadBufferSizeReply &)>;
        void readBufferSize(const ReadBufferSizeCb &cb) {
            bte_hci_read_buffer_size(m_hci, wrap<TAG>(cb));
        }

        using ReadBdAddrCb =
            std::function<void(const BteHciReadBdAddrReply &)>;
        void readBdAddr(const ReadBdAddrCb &cb) {
            bte_hci_read_bd_addr(m_hci, wrap<TAG>(cb));
        }

        using VendorCommandCb = std::function<void(const Buffer &)>;
        void vendorCommand(uint16_t ocf, const Buffer &buffer,
                           const VendorCommandCb &cb) {
            bte_hci_vendor_command(m_hci, ocf,
                                   buffer.data(), uint8_t(buffer.size()),
                                   wrap<TAG, BteBuffer>(cb));
        }

    private:
        std::unordered_map<Tag, std::any> m_callbacks;

        struct Callbacks {
            static Hci *_this(void *cb_data) {
                return &static_cast<Client*>(cb_data)->m_hci;
            }
            static void initialized(BteHci *hci, bool success, void *cb_data) {
                _this(cb_data)->m_initializedCb(success);
            }

            static void inquiryStatus(BteHci *hci, const BteHciReply *reply,
                                      void *cb_data) {
                _this(cb_data)->m_inquiryCb.first(*reply);
            }
            static void inquiry(BteHci *hci, const BteHciInquiryReply *reply,
                                void *cb_data) {
                _this(cb_data)->m_inquiryCb.second(*reply);
            }
            static void createConnection(
                BteHci *hci, const BteHciCreateConnectionReply *reply,
                void *cb_data) {
                _this(cb_data)->m_createConnectionCallbacks[reply->address](
                    *reply);
            }
            static bool connectionRequest(BteHci *hci,
                                          const BteBdAddr *address,
                                          const BteClassOfDevice *cod,
                                          uint8_t link_type,
                                          void *cb_data) {
                return _this(cb_data)->m_connectionRequestCb(
                    *address, *cod, link_type);
            }
            static bool linkKeyRequest(BteHci *hci, const BteBdAddr *address,
                                       void *cb_data) {
                return _this(cb_data)->m_linkKeyRequestCb(*address);
            }
            static bool pinCodeRequest(BteHci *hci, const BteBdAddr *address,
                                       void *cb_data) {
                return _this(cb_data)->m_pinCodeRequestCb(*address);
            }
            static void authRequested(
                BteHci *hci, const BteHciAuthRequestedReply *reply,
                void *cb_data) {
                _this(cb_data)->m_authRequestedCallbacks[reply->conn_handle](
                    *reply);
            }
            static void readRemoteName(
                BteHci *hci, const BteHciReadRemoteNameReply *reply,
                void *cb_data) {
                _this(cb_data)->m_readRemoteNameCallbacks[reply->address](
                    *reply);
            }
            static void readRemoteFeatures(
                BteHci *hci, const BteHciReadRemoteFeaturesReply *reply,
                void *cb_data) {
                _this(cb_data)->m_readRemoteFeaturesCallbacks[
                    reply->conn_handle](*reply);
            }
            static void readRemoteVersionInfo(
                BteHci *hci, const BteHciReadRemoteVersionInfoReply *reply,
                void *cb_data) {
                _this(cb_data)->m_readRemoteVersionInfoCallbacks[
                    reply->conn_handle](*reply);
            }
            static void readClockOffset(
                BteHci *hci, const BteHciReadClockOffsetReply *reply,
                void *cb_data) {
                _this(cb_data)->m_readClockOffsetCallbacks[reply->conn_handle](
                    *reply);
            }
            static bool modeChange(BteHci *hci,
                                   const BteHciModeChangeReply *reply,
                                   void *cb_data) {
                return _this(cb_data)->m_modeChangeCb(*reply);
            }
        };

        Hci(BteHci *hci): m_hci(hci) {}

        friend class Client;
        InitializedCb m_initializedCb;
        std::pair<DoneCb, InquiryCb> m_inquiryCb;
        std::unordered_map<BteBdAddr, CreateConnectionCb>
            m_createConnectionCallbacks;
        std::unordered_map<BteHciConnHandle, AuthRequestedCb>
            m_authRequestedCallbacks;
        std::unordered_map<BteBdAddr, ReadRemoteNameCb>
            m_readRemoteNameCallbacks;
        std::unordered_map<BteHciConnHandle, ReadRemoteFeaturesCb>
            m_readRemoteFeaturesCallbacks;
        std::unordered_map<BteHciConnHandle, ReadRemoteVersionInfoCb>
            m_readRemoteVersionInfoCallbacks;
        std::unordered_map<BteHciConnHandle, ReadClockOffsetCb>
            m_readClockOffsetCallbacks;
        ConnectionRequestCb m_connectionRequestCb;
        LinkKeyRequestCb m_linkKeyRequestCb;
        PinCodeRequestCb m_pinCodeRequestCb;
        ModeChangeCb m_modeChangeCb;
        BteHci *m_hci;
    };

    Hci &hci() { return m_hci; }

private:
    void init() {
        bte_client_set_userdata(m_client, this);
    }

    friend class Hci;
    BteClient *m_client;
    Hci m_hci;
};

} // namespace Bte

#endif /* BTE_TESTS_BTE_CPP_H */
