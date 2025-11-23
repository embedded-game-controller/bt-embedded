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
        enum Tag {
            Nop,
            InquiryCancel,
            ExitPeriodicInquiry,
            LinkKeyReqReply,
            LinkKeyReqNegReply,
            PinCodeReqReply,
            PinCodeReqNegReply,
            SetEventMask,
            Reset,
            WritePinType,
            ReadPinType,
            ReadStoredLinkKey,
            WriteStoredLinkKey,
            DeleteStoredLinkKey,
            WriteLocalName,
            ReadLocalName,
            WritePageTimeout,
            ReadPageTimeout,
            WriteScanEnable,
            ReadScanEnable,
            WriteAuthEnable,
            ReadAuthEnable,
            WriteClassOfDevice,
            WriteInquiryScanType,
            ReadInquiryScanType,
            WriteInquiryMode,
            ReadInquiryMode,
            WritePageScanType,
            ReadPageScanType,
            ReadLocalVersion,
            ReadLocalFeatures,
            ReadBufferSize,
            ReadBdAddr,
            VendorCommand,
        };
        template <typename T> static void *tag(T x) {
            return *reinterpret_cast<void**>(&x);
        }

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
            bte_hci_nop(m_hci, wrap<Nop>(cb));
        }

        using InquiryCb = std::function<void(const BteHciInquiryReply &)>;
        void inquiry(uint32_t lap, uint8_t len, uint8_t max_resp,
                     const DoneCb &statusCb, const InquiryCb &cb) {
            m_inquiryCb = {statusCb, cb};
            bte_hci_inquiry(m_hci, lap, len, max_resp,
                            &Hci::Callbacks::inquiryStatus,
                            &Hci::Callbacks::inquiry);
        }

        void inquiryCancel(const DoneCb &cb) {
            bte_hci_inquiry_cancel(m_hci, wrap<InquiryCancel>(cb));
        }

        void periodicInquiry(uint16_t min_period, uint16_t max_period,
                             uint32_t lap, uint8_t len, uint8_t max_resp,
                             const DoneCb &statusCb, const InquiryCb &cb) {
            m_inquiryCb = {statusCb, cb};
            bte_hci_periodic_inquiry(m_hci, min_period, max_period,
                                     lap, len, max_resp,
                                     &Hci::Callbacks::inquiryStatus,
                                     &Hci::Callbacks::inquiry);
        }

        void exitPeriodicInquiry(const DoneCb &cb) {
            bte_hci_exit_periodic_inquiry(m_hci,
                                          wrap<ExitPeriodicInquiry>(cb));
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
            bte_hci_link_key_req_reply(m_hci, &address, &key,
                                       wrap<LinkKeyReqReply>(cb));
        }

        void linkKeyReqNegReply(const BteBdAddr &address,
                                const LinkKeyReqReplyCb &cb) {
            bte_hci_link_key_req_neg_reply(m_hci, &address,
                                           wrap<LinkKeyReqNegReply>(cb));
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
                                       wrap<PinCodeReqReply>(cb));
        }

        void pinCodeReqNegReply(const BteBdAddr &address,
                                const PinCodeReqReplyCb &cb) {
            bte_hci_pin_code_req_neg_reply(m_hci, &address,
                                           wrap<PinCodeReqNegReply>(cb));
        }

        void setEventMask(BteHciEventMask mask, const DoneCb &cb) {
            bte_hci_set_event_mask(m_hci, mask, wrap<SetEventMask>(cb));
        }

        void reset(const DoneCb &cb) {
            bte_hci_reset(m_hci, wrap<Reset>(cb));
        }

        void writePinType(uint8_t pin_type, const DoneCb &cb) {
            bte_hci_write_pin_type(m_hci, pin_type, wrap<WritePinType>(cb));
        }

        using ReadPinTypeCb =
            std::function<void(const BteHciReadPinTypeReply &)>;
        void readPinType(const ReadPinTypeCb &cb) {
            bte_hci_read_pin_type(m_hci, wrap<ReadPinType>(cb));
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
                wrap<ReadStoredLinkKey,
                     const BteHciReadStoredLinkKeyReply>(cb));
        }
        void readStoredLinkKey(const ReadStoredLinkKeyCb &cb) {
            bte_hci_read_stored_link_key(m_hci, nullptr,
                wrap<ReadStoredLinkKey,
                     const BteHciReadStoredLinkKeyReply>(cb));
        }

        using WriteStoredLinkKeyCb =
            std::function<void(const BteHciWriteStoredLinkKeyReply &)>;
        void writeStoredLinkKey(
            const std::span<const BteHciStoredLinkKey> &keys,
            const WriteStoredLinkKeyCb &cb) {
            bte_hci_write_stored_link_key(m_hci, keys.size(), keys.data(),
                                          wrap<WriteStoredLinkKey>(cb));
        }

        using DeleteStoredLinkKeyCb =
            std::function<void(const BteHciDeleteStoredLinkKeyReply &)>;
        void deleteStoredLinkKey(const BteBdAddr &address,
                                 const DeleteStoredLinkKeyCb &cb) {
            bte_hci_delete_stored_link_key(
                m_hci, &address, wrap<DeleteStoredLinkKey>(cb));
        }
        void deleteStoredLinkKey(const DeleteStoredLinkKeyCb &cb) {
            bte_hci_delete_stored_link_key(
                m_hci, nullptr, wrap<DeleteStoredLinkKey>(cb));
        }

        void writeLocalName(const std::string &name, const DoneCb &cb) {
            bte_hci_write_local_name(m_hci, name.c_str(),
                                     wrap<WriteLocalName>(cb));
        }

        using ReadLocalNameCb =
            std::function<void(const BteHciReadLocalNameReply &)>;
        void readLocalName(const ReadLocalNameCb &cb) {
            bte_hci_read_local_name(m_hci, wrap<ReadLocalName>(cb));
        }

        void writePageTimeout(uint16_t page_timeout, const DoneCb &cb) {
            bte_hci_write_page_timeout(m_hci, page_timeout,
                                       wrap<WritePageTimeout>(cb));
        }

        using ReadPageTimeoutCb =
            std::function<void(const BteHciReadPageTimeoutReply &)>;
        void readPageTimeout(const ReadPageTimeoutCb &cb) {
            bte_hci_read_page_timeout(m_hci, wrap<ReadPageTimeout>(cb));
        }

        void writeScanEnable(uint8_t scan_enable, const DoneCb &cb) {
            bte_hci_write_scan_enable(m_hci, scan_enable,
                                      wrap<WriteScanEnable>(cb));
        }

        using ReadScanEnableCb =
            std::function<void(const BteHciReadScanEnableReply &)>;
        void readScanEnable(const ReadScanEnableCb &cb) {
            bte_hci_read_scan_enable(m_hci, wrap<ReadScanEnable>(cb));
        }

        void writeAuthEnable(uint8_t auth_enable, const DoneCb &cb) {
            bte_hci_write_auth_enable(m_hci, auth_enable,
                                      wrap<WriteAuthEnable>(cb));
        }

        using ReadAuthEnableCb =
            std::function<void(const BteHciReadAuthEnableReply &)>;
        void readAuthEnable(const ReadAuthEnableCb &cb) {
            bte_hci_read_auth_enable(m_hci, wrap<ReadAuthEnable>(cb));
        }

        void writeClassOfDevice(const BteClassOfDevice &cod, const DoneCb &cb) {
            bte_hci_write_class_of_device(m_hci, &cod,
                                          wrap<WriteClassOfDevice>(cb));
        }

        void writeInquiryScanType(uint8_t inquiry_scan_type,
                                  const DoneCb &cb) {
            bte_hci_write_inquiry_scan_type(
                m_hci, inquiry_scan_type, wrap<WriteInquiryScanType>(cb));
        }

        using ReadInquiryScanTypeCb =
            std::function<void(const BteHciReadInquiryScanTypeReply &)>;
        void readInquiryScanType(const ReadInquiryScanTypeCb &cb) {
            bte_hci_read_inquiry_scan_type(
                m_hci, wrap<ReadInquiryScanType>(cb));
        }

        void writeInquiryMode(uint8_t inquiry_mode, const DoneCb &cb) {
            bte_hci_write_inquiry_mode(m_hci, inquiry_mode,
                                       wrap<WriteInquiryMode>(cb));
        }

        using ReadInquiryModeCb =
            std::function<void(const BteHciReadInquiryModeReply &)>;
        void readInquiryMode(const ReadInquiryModeCb &cb) {
            bte_hci_read_inquiry_mode(m_hci, wrap<ReadInquiryMode>(cb));
        }

        void writePageScanType(uint8_t page_scan_type, const DoneCb &cb) {
            bte_hci_write_page_scan_type(m_hci, page_scan_type,
                                         wrap<WritePageScanType>(cb));
        }

        using ReadPageScanTypeCb =
            std::function<void(const BteHciReadPageScanTypeReply &)>;
        void readPageScanType(const ReadPageScanTypeCb &cb) {
            bte_hci_read_page_scan_type(m_hci, wrap<ReadPageScanType>(cb));
        }

        using ReadLocalVersionCb =
            std::function<void(const BteHciReadLocalVersionReply &)>;
        void readLocalVersion(const ReadLocalVersionCb &cb) {
            bte_hci_read_local_version(m_hci, wrap<ReadLocalVersion>(cb));
        }

        using ReadLocalFeaturesCb =
            std::function<void(const BteHciReadLocalFeaturesReply &)>;
        void readLocalFeatures(const ReadLocalFeaturesCb &cb) {
            bte_hci_read_local_features(m_hci, wrap<ReadLocalFeatures>(cb));
        }

        using ReadBufferSizeCb =
            std::function<void(const BteHciReadBufferSizeReply &)>;
        void readBufferSize(const ReadBufferSizeCb &cb) {
            bte_hci_read_buffer_size(m_hci, wrap<ReadBufferSize>(cb));
        }

        using ReadBdAddrCb =
            std::function<void(const BteHciReadBdAddrReply &)>;
        void readBdAddr(const ReadBdAddrCb &cb) {
            bte_hci_read_bd_addr(m_hci, wrap<ReadBdAddr>(cb));
        }

        using VendorCommandCb = std::function<void(const Buffer &)>;
        void vendorCommand(uint16_t ocf, const Buffer &buffer,
                           const VendorCommandCb &cb) {
            bte_hci_vendor_command(m_hci, ocf,
                                   buffer.data(), uint8_t(buffer.size()),
                                   wrap<VendorCommand, BteBuffer>(cb));
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
            static bool linkKeyRequest(BteHci *hci, const BteBdAddr *address,
                                       void *cb_data) {
                return _this(cb_data)->m_linkKeyRequestCb(*address);
            }
            static bool pinCodeRequest(BteHci *hci, const BteBdAddr *address,
                                       void *cb_data) {
                return _this(cb_data)->m_pinCodeRequestCb(*address);
            }
        };

        Hci(BteHci *hci): m_hci(hci) {}

        friend class Client;
        InitializedCb m_initializedCb;
        std::pair<DoneCb, InquiryCb> m_inquiryCb;
        LinkKeyRequestCb m_linkKeyRequestCb;
        PinCodeRequestCb m_pinCodeRequestCb;
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
