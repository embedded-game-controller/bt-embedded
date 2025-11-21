#ifndef BTE_TESTS_BTE_CPP_H
#define BTE_TESTS_BTE_CPP_H

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"

#include <cstring>
#include <functional>
#include <span>
#include <string>

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
        using InitializedCb = std::function<void(bool)>;
        void onInitialized(const InitializedCb &cb) {
            m_initializedCb = cb;
            bte_hci_on_initialized(m_hci, &Hci::Callbacks::initialized);
        }

        using DoneCb = std::function<void(const BteHciReply &)>;
        void nop(const DoneCb &cb) {
            m_nopCb = cb;
            bte_hci_nop(m_hci, &Hci::Callbacks::nop);
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
            m_inquiryCancelCb = cb;
            bte_hci_inquiry_cancel(m_hci, &Hci::Callbacks::inquiryCancel);
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
            m_exitPeriodicInquiryCb = cb;
            bte_hci_exit_periodic_inquiry(
                m_hci, &Hci::Callbacks::exitPeriodicInquiry);
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
            m_linkKeyReqReplyCb = cb;
            bte_hci_link_key_req_reply(m_hci, &address, &key,
                                       &Hci::Callbacks::linkKeyReqReply);
        }

        void linkKeyReqNegReply(const BteBdAddr &address,
                                const LinkKeyReqReplyCb &cb) {
            m_linkKeyReqNegReplyCb = cb;
            bte_hci_link_key_req_neg_reply(m_hci, &address,
                                           &Hci::Callbacks::linkKeyReqNegReply);
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
            m_pinCodeReqReplyCb = cb;
            bte_hci_pin_code_req_reply(m_hci, &address, pin.data(), pin.size(),
                                       &Hci::Callbacks::pinCodeReqReply);
        }

        void pinCodeReqNegReply(const BteBdAddr &address,
                                const PinCodeReqReplyCb &cb) {
            m_pinCodeReqNegReplyCb = cb;
            bte_hci_pin_code_req_neg_reply(m_hci, &address,
                                           &Hci::Callbacks::pinCodeReqNegReply);
        }

        void setEventMask(BteHciEventMask mask, const DoneCb &cb) {
            m_setEventMaskCb = cb;
            bte_hci_set_event_mask(m_hci, mask, &Hci::Callbacks::setEventMask);
        }

        void reset(const DoneCb &cb) {
            m_resetCb = cb;
            bte_hci_reset(m_hci, &Hci::Callbacks::reset);
        }

        void writePinType(uint8_t pin_type, const DoneCb &cb) {
            m_writePinTypeCb = cb;
            bte_hci_write_pin_type(m_hci, pin_type,
                                   &Hci::Callbacks::writePinType);
        }

        using ReadPinTypeCb =
            std::function<void(const BteHciReadPinTypeReply &)>;
        void readPinType(const ReadPinTypeCb &cb) {
            m_readPinTypeCb = cb;
            bte_hci_read_pin_type(m_hci, &Hci::Callbacks::readPinType);
        }

        struct ReadStoredLinkKeyReply {
            uint8_t status;
            uint16_t max_keys;
            std::span<const BteHciStoredLinkKey> stored_keys;
        };
        using ReadStoredLinkKeyCb =
            std::function<void(const ReadStoredLinkKeyReply &)>;
        void readStoredLinkKey(const BteBdAddr &address,
                               const ReadStoredLinkKeyCb &cb) {
            m_readStoredLinkKeyCb = cb;
            bte_hci_read_stored_link_key(m_hci, &address,
                                         &Hci::Callbacks::readStoredLinkKey);
        }
        void readStoredLinkKey(const ReadStoredLinkKeyCb &cb) {
            m_readStoredLinkKeyCb = cb;
            bte_hci_read_stored_link_key(m_hci, nullptr,
                                         &Hci::Callbacks::readStoredLinkKey);
        }

        using WriteStoredLinkKeyCb =
            std::function<void(const BteHciWriteStoredLinkKeyReply &)>;
        void writeStoredLinkKey(
            const std::span<const BteHciStoredLinkKey> &keys,
            const WriteStoredLinkKeyCb &cb) {
            m_writeStoredLinkKeyCb = cb;
            bte_hci_write_stored_link_key(m_hci, keys.size(), keys.data(),
                                          &Hci::Callbacks::writeStoredLinkKey);
        }

        using DeleteStoredLinkKeyCb =
            std::function<void(const BteHciDeleteStoredLinkKeyReply &)>;
        void deleteStoredLinkKey(const BteBdAddr &address,
                                 const DeleteStoredLinkKeyCb &cb) {
            m_deleteStoredLinkKeyCb = cb;
            bte_hci_delete_stored_link_key(
                m_hci, &address, &Hci::Callbacks::deleteStoredLinkKey);
        }
        void deleteStoredLinkKey(const DeleteStoredLinkKeyCb &cb) {
            m_deleteStoredLinkKeyCb = cb;
            bte_hci_delete_stored_link_key(
                m_hci, nullptr, &Hci::Callbacks::deleteStoredLinkKey);
        }

        void writeLocalName(const std::string &name, const DoneCb &cb) {
            m_writeLocalNameCb = cb;
            bte_hci_write_local_name(m_hci, name.c_str(),
                                     &Hci::Callbacks::writeLocalName);
        }

        using ReadLocalNameCb =
            std::function<void(const BteHciReadLocalNameReply &)>;
        void readLocalName(const ReadLocalNameCb &cb) {
            m_readLocalNameCb = cb;
            bte_hci_read_local_name(m_hci, &Hci::Callbacks::readLocalName);
        }

        void writePageTimeout(uint16_t page_timeout, const DoneCb &cb) {
            m_writePageTimeoutCb = cb;
            bte_hci_write_page_timeout(m_hci, page_timeout,
                                       &Hci::Callbacks::writePageTimeout);
        }

        using ReadPageTimeoutCb =
            std::function<void(const BteHciReadPageTimeoutReply &)>;
        void readPageTimeout(const ReadPageTimeoutCb &cb) {
            m_readPageTimeoutCb = cb;
            bte_hci_read_page_timeout(m_hci, &Hci::Callbacks::readPageTimeout);
        }

        void writeScanEnable(uint8_t scan_enable, const DoneCb &cb) {
            m_writeScanEnableCb = cb;
            bte_hci_write_scan_enable(m_hci, scan_enable,
                                   &Hci::Callbacks::writeScanEnable);
        }

        using ReadScanEnableCb =
            std::function<void(const BteHciReadScanEnableReply &)>;
        void readScanEnable(const ReadScanEnableCb &cb) {
            m_readScanEnableCb = cb;
            bte_hci_read_scan_enable(m_hci, &Hci::Callbacks::readScanEnable);
        }

        void writeAuthEnable(uint8_t auth_enable, const DoneCb &cb) {
            m_writeAuthEnableCb = cb;
            bte_hci_write_auth_enable(m_hci, auth_enable,
                                      &Hci::Callbacks::writeAuthEnable);
        }

        using ReadAuthEnableCb =
            std::function<void(const BteHciReadAuthEnableReply &)>;
        void readAuthEnable(const ReadAuthEnableCb &cb) {
            m_readAuthEnableCb = cb;
            bte_hci_read_auth_enable(m_hci, &Hci::Callbacks::readAuthEnable);
        }

        void writeClassOfDevice(const BteClassOfDevice &cod, const DoneCb &cb) {
            m_writeClassOfDevice = cb;
            bte_hci_write_class_of_device(m_hci, &cod,
                                          &Hci::Callbacks::writeClassOfDevice);
        }

        using ReadLocalVersionCb =
            std::function<void(const BteHciReadLocalVersionReply &)>;
        void readLocalVersion(const ReadLocalVersionCb &cb) {
            m_readLocalVersionCb = cb;
            bte_hci_read_local_version(m_hci,
                                       &Hci::Callbacks::readLocalVersion);
        }

        using ReadLocalFeaturesCb =
            std::function<void(const BteHciReadLocalFeaturesReply &)>;
        void readLocalFeatures(const ReadLocalFeaturesCb &cb) {
            m_readLocalFeaturesCb = cb;
            bte_hci_read_local_features(m_hci,
                                        &Hci::Callbacks::readLocalFeatures);
        }

        using ReadBufferSizeCb =
            std::function<void(const BteHciReadBufferSizeReply &)>;
        void readBufferSize(const ReadBufferSizeCb &cb) {
            m_readBufferSizeCb = cb;
            bte_hci_read_buffer_size(m_hci, &Hci::Callbacks::readBufferSize);
        }

        using ReadBdAddrCb =
            std::function<void(const BteHciReadBdAddrReply &)>;
        void readBdAddr(const ReadBdAddrCb &cb) {
            m_readBdAddrCb = cb;
            bte_hci_read_bd_addr(m_hci, &Hci::Callbacks::readBdAddr);
        }

        using VendorCommandCb = std::function<void(const Buffer &)>;
        void vendorCommand(uint16_t ocf, const Buffer &buffer,
                           const VendorCommandCb &cb) {
            m_vendorCommandCb = cb;
            bte_hci_vendor_command(m_hci, ocf,
                                   buffer.data(), uint8_t(buffer.size()),
                                   &Hci::Callbacks::vendorCommand);
        }

    private:
        struct Callbacks {
            static Hci *_this(void *cb_data) {
                return &static_cast<Client*>(cb_data)->m_hci;
            }
            static void initialized(BteHci *hci, bool success, void *cb_data) {
                _this(cb_data)->m_initializedCb(success);
            }

            static void nop(BteHci *hci, const BteHciReply *reply, void *cb_data) {
                _this(cb_data)->m_nopCb(*reply);
            }
            static void inquiryStatus(BteHci *hci, const BteHciReply *reply,
                                      void *cb_data) {
                _this(cb_data)->m_inquiryCb.first(*reply);
            }
            static void inquiry(BteHci *hci, const BteHciInquiryReply *reply,
                                void *cb_data) {
                _this(cb_data)->m_inquiryCb.second(*reply);
            }
            static void inquiryCancel(BteHci *hci, const BteHciReply *reply,
                                      void *cb_data) {
                _this(cb_data)->m_inquiryCancelCb(*reply);
            }
            static void exitPeriodicInquiry(BteHci *hci,
                                            const BteHciReply *reply,
                                            void *cb_data) {
                _this(cb_data)->m_exitPeriodicInquiryCb(*reply);
            }
            static bool linkKeyRequest(BteHci *hci, const BteBdAddr *address,
                                       void *cb_data) {
                return _this(cb_data)->m_linkKeyRequestCb(*address);
            }
            static void linkKeyReqReply(BteHci *hci,
                                        const BteHciLinkKeyReqReply *reply,
                                        void *cb_data) {
                _this(cb_data)->m_linkKeyReqReplyCb(*reply);
            }
            static void linkKeyReqNegReply(BteHci *hci,
                                           const BteHciLinkKeyReqReply *reply,
                                           void *cb_data) {
                _this(cb_data)->m_linkKeyReqNegReplyCb(*reply);
            }
            static bool pinCodeRequest(BteHci *hci, const BteBdAddr *address,
                                       void *cb_data) {
                return _this(cb_data)->m_pinCodeRequestCb(*address);
            }
            static void pinCodeReqReply(BteHci *hci,
                                        const BteHciPinCodeReqReply *reply,
                                        void *cb_data) {
                _this(cb_data)->m_pinCodeReqReplyCb(*reply);
            }
            static void pinCodeReqNegReply(BteHci *hci,
                                           const BteHciPinCodeReqReply *reply,
                                           void *cb_data) {
                _this(cb_data)->m_pinCodeReqNegReplyCb(*reply);
            }
            static void setEventMask(BteHci *hci, const BteHciReply *reply,
                                     void *cb_data) {
                _this(cb_data)->m_setEventMaskCb(*reply);
            }
            static void reset(BteHci *hci, const BteHciReply *reply,
                              void *cb_data) {
                _this(cb_data)->m_resetCb(*reply);
            }
            static void writePinType(BteHci *hci, const BteHciReply *reply,
                                     void *cb_data) {
                _this(cb_data)->m_writePinTypeCb(*reply);
            }
            static void readPinType(BteHci *hci,
                                    const BteHciReadPinTypeReply *reply,
                                    void *cb_data) {
                _this(cb_data)->m_readPinTypeCb(*reply);
            }
            static void readStoredLinkKey(BteHci *hci,
                const BteHciReadStoredLinkKeyReply *reply, void *cb_data) {
                _this(cb_data)->m_readStoredLinkKeyCb({
                    reply->status, reply->max_keys,
                    {reply->stored_keys, reply->num_keys},
                });
            }
            static void
            writeStoredLinkKey(BteHci *hci,
                const BteHciWriteStoredLinkKeyReply *reply, void *cb_data) {
                _this(cb_data)->m_writeStoredLinkKeyCb(*reply);
            }
            static void
            deleteStoredLinkKey(BteHci *hci,
                const BteHciDeleteStoredLinkKeyReply *reply, void *cb_data) {
                _this(cb_data)->m_deleteStoredLinkKeyCb(*reply);
            }
            static void writeLocalName(BteHci *hci, const BteHciReply *reply,
                                       void *cb_data) {
                _this(cb_data)->m_writeLocalNameCb(*reply);
            }
            static void readLocalName(BteHci *hci,
                                      const BteHciReadLocalNameReply *reply,
                                      void *cb_data) {
                _this(cb_data)->m_readLocalNameCb(*reply);
            }
            static void writePageTimeout(BteHci *hci, const BteHciReply *reply,
                                         void *cb_data) {
                _this(cb_data)->m_writePageTimeoutCb(*reply);
            }
            static void readPageTimeout(BteHci *hci,
                                        const BteHciReadPageTimeoutReply *reply,
                                        void *cb_data) {
                _this(cb_data)->m_readPageTimeoutCb(*reply);
            }
            static void writeScanEnable(BteHci *hci, const BteHciReply *reply,
                                        void *cb_data) {
                _this(cb_data)->m_writeScanEnableCb(*reply);
            }
            static void readScanEnable(BteHci *hci,
                                       const BteHciReadScanEnableReply *reply,
                                       void *cb_data) {
                _this(cb_data)->m_readScanEnableCb(*reply);
            }
            static void writeAuthEnable(BteHci *hci, const BteHciReply *reply,
                                        void *cb_data) {
                _this(cb_data)->m_writeAuthEnableCb(*reply);
            }
            static void readAuthEnable(BteHci *hci,
                                       const BteHciReadAuthEnableReply *reply,
                                       void *cb_data) {
                _this(cb_data)->m_readAuthEnableCb(*reply);
            }
            static void writeClassOfDevice(BteHci *hci,
                                           const BteHciReply *reply,
                                           void *cb_data) {
                _this(cb_data)->m_writeClassOfDevice(*reply);
            }
            static void readLocalVersion(BteHci *hci,
                const BteHciReadLocalVersionReply *reply, void *cb_data) {
                _this(cb_data)->m_readLocalVersionCb(*reply);
            }
            static void readLocalFeatures(BteHci *hci,
                const BteHciReadLocalFeaturesReply *reply, void *cb_data) {
                _this(cb_data)->m_readLocalFeaturesCb(*reply);
            }
            static void readBufferSize(BteHci *hci,
                const BteHciReadBufferSizeReply *reply, void *cb_data) {
                _this(cb_data)->m_readBufferSizeCb(*reply);
            }
            static void readBdAddr(BteHci *hci,
                const BteHciReadBdAddrReply *reply, void *cb_data) {
                _this(cb_data)->m_readBdAddrCb(*reply);
            }
            static void vendorCommand(BteHci *hci, BteBuffer *reply,
                                      void *cb_data) {
                _this(cb_data)->m_vendorCommandCb(reply);
            }
        };

        Hci(BteHci *hci): m_hci(hci) {}

        friend class Client;
        InitializedCb m_initializedCb;
        DoneCb m_nopCb;
        std::pair<DoneCb, InquiryCb> m_inquiryCb;
        DoneCb m_inquiryCancelCb;
        DoneCb m_exitPeriodicInquiryCb;
        LinkKeyRequestCb m_linkKeyRequestCb;
        LinkKeyReqReplyCb m_linkKeyReqReplyCb;
        LinkKeyReqReplyCb m_linkKeyReqNegReplyCb;
        PinCodeRequestCb m_pinCodeRequestCb;
        PinCodeReqReplyCb m_pinCodeReqReplyCb;
        PinCodeReqReplyCb m_pinCodeReqNegReplyCb;
        DoneCb m_setEventMaskCb;
        DoneCb m_resetCb;
        DoneCb m_writePinTypeCb;
        ReadPinTypeCb m_readPinTypeCb;
        ReadStoredLinkKeyCb m_readStoredLinkKeyCb;
        WriteStoredLinkKeyCb m_writeStoredLinkKeyCb;
        DeleteStoredLinkKeyCb m_deleteStoredLinkKeyCb;
        DoneCb m_writeLocalNameCb;
        ReadLocalNameCb m_readLocalNameCb;
        DoneCb m_writePageTimeoutCb;
        ReadPageTimeoutCb m_readPageTimeoutCb;
        DoneCb m_writeScanEnableCb;
        ReadScanEnableCb m_readScanEnableCb;
        DoneCb m_writeAuthEnableCb;
        ReadAuthEnableCb m_readAuthEnableCb;
        DoneCb m_writeClassOfDevice;
        ReadLocalVersionCb m_readLocalVersionCb;
        ReadLocalFeaturesCb m_readLocalFeaturesCb;
        ReadBufferSizeCb m_readBufferSizeCb;
        ReadBdAddrCb m_readBdAddrCb;
        VendorCommandCb m_vendorCommandCb;
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
