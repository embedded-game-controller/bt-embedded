#ifndef BTE_TESTS_BTE_L2CAP_CPP_H
#define BTE_TESTS_BTE_L2CAP_CPP_H

#include "bte_cpp.h"

#include "bt-embedded/l2cap.h"
#include "bt-embedded/l2cap_server.h"

#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>

inline bool operator==(const BteL2capConnectionResponse &a,
                       const BteL2capConnectionResponse &b)
{
    return a.remote_channel_id == b.remote_channel_id &&
        a.local_channel_id == b.local_channel_id &&
        a.result == b.result && a.status == b.status;
}

inline bool operator==(const BteL2capConfigQos &a, const BteL2capConfigQos &b)
{
    return a.flags == b.flags && a.service_type == b.service_type &&
        a.token_rate == b.token_rate &&
        a.token_bucket_size == b.token_bucket_size &&
        a.peak_bandwith == b.peak_bandwith &&
        a.access_latency == b.access_latency &&
        a.delay_variation == b.delay_variation;
}

inline bool operator==(const BteL2capConfigRetxFlow &a,
                       const BteL2capConfigRetxFlow &b)
{
    return a.mode == b.mode && a.tx_window_size == b.tx_window_size &&
        a.max_transmit == b.max_transmit && a.retx_timeout == b.retx_timeout &&
        a.monitor_timeout == b.monitor_timeout &&
        a.max_pdu_size == b.max_pdu_size;
}

inline bool operator==(const BteL2capConfigExtFlow &a,
                       const BteL2capConfigExtFlow &b)
{
    return a.identifier == b.identifier && a.service_type == b.service_type &&
        a.max_sdu_size == b.max_sdu_size &&
        a.sdu_inter_time == b.sdu_inter_time &&
        a.access_latency == b.access_latency &&
        a.flush_timeout == b.flush_timeout;
}

class TestL2capConfig;
class TestL2capFixtureConnected;
class TestL2capFixtureConfigured;

namespace Bte {

class L2capServer;

class L2cap {
public:
    using ConnectCb = std::function<void(
        std::optional<L2cap> l2cap, const BteL2capConnectionResponse &reply)>;
    static void newOutgoing(
        Client &client, const BteBdAddr &address, BteL2capPsm psm,
        const std::optional<BteHciConnectParams> &params,
        const ConnectCb &cb) {
        auto *f = new ConnectCb(cb);
        bte_l2cap_new_outgoing(client.m_client, &address, psm,
                               params ? &params.value() : nullptr,
                               &L2cap::Callbacks::connect, f);
    }

    L2cap(BteL2cap *l2cap = nullptr): m_l2cap(l2cap) {}
    L2cap(const L2cap &other): m_l2cap(bte_l2cap_ref(other.m_l2cap)) {}
    ~L2cap() { if (m_l2cap) bte_l2cap_unref(m_l2cap); }

    L2cap &operator=(const L2cap &other) {
        if (m_l2cap) bte_l2cap_unref(m_l2cap);
        m_l2cap = bte_l2cap_ref(other.m_l2cap);
        return *this;
    }

    bool isValid() const { return m_l2cap != nullptr; }

    BteConnHandle connectionHandle() const {
        return bte_l2cap_get_connection_handle(m_l2cap);
    }

    BteL2capPsm psm() const {
        return bte_l2cap_get_psm(m_l2cap);
    }

    uint16_t mtu() const { return bte_l2cap_get_mtu(m_l2cap); }
    uint16_t remoteMtu() const { return bte_l2cap_get_remote_mtu(m_l2cap); }

    BteL2capState state() const {
        return bte_l2cap_get_state(m_l2cap);
    }

    using StateChangedCb = std::function<void(BteL2capState state)>;
    void onStateChanged(const StateChangedCb &cb)
    {
        m_onStateChanged = cb;
        bte_l2cap_set_userdata(m_l2cap, this);
        bte_l2cap_on_state_changed(m_l2cap, &L2cap::Callbacks::onStateChanged);
    }

    struct ConfigureParams {
        ConfigureParams(): p{0} {}
        ConfigureParams(const BteL2capConfigureParams &params): p(params) {
            update();
        }

        ConfigureParams(const ConfigureParams &o): p(o.p) {
            update();
        }

        void setMtu(uint16_t mtu) {
            p.field_mask |= BTE_L2CAP_CONFIG_MTU;
            p.mtu = mtu;
        }
        uint16_t mtu() const { return p.mtu; }

        void setFlushTimeout(uint16_t flushTimeout) {
            p.field_mask |= BTE_L2CAP_CONFIG_FLUSH_TIMEOUT;
            p.flush_timeout = flushTimeout;
        }
        uint16_t flushTimeout() const { return p.flush_timeout; }

        void setFrameCheckSequence(uint8_t seq) {
            p.field_mask |= BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ;
            p.frame_check_sequence = seq;
        }

        void setMaxWindowSize(uint16_t size) {
            p.field_mask |= BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE;
            p.max_window_size = size;
        }

        void setQos(const BteL2capConfigQos &qos) {
            p.field_mask |= BTE_L2CAP_CONFIG_QOS;
            p.qos = &qos;
            update();
        }
        const BteL2capConfigQos &qos() const {
            return *p.qos;
        }

        void setRetxFlow(const BteL2capConfigRetxFlow &retxFlow) {
            p.field_mask |= BTE_L2CAP_CONFIG_RETX_FLOW;
            p.retx_flow = &retxFlow;
            update();
        }
        const BteL2capConfigRetxFlow &retxFlow() const {
            return *p.retx_flow;
        }

        void setExtFlow(const BteL2capConfigExtFlow &extFlow) {
            p.field_mask |= BTE_L2CAP_CONFIG_EXT_FLOW;
            p.ext_flow = &extFlow;
            update();
        }
        const BteL2capConfigExtFlow &extFlow() const {
            return *p.ext_flow;
        }

        bool operator==(const ConfigureParams &o) const {
            if (p.field_mask != o.p.field_mask) return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_MTU && p.mtu != o.p.mtu)
                return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_FLUSH_TIMEOUT &&
                p.flush_timeout != o.p.flush_timeout)
                return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ &&
                p.frame_check_sequence != o.p.frame_check_sequence)
                return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE &&
                p.max_window_size != o.p.max_window_size)
                return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_QOS && *p.qos != *o.p.qos)
                return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_RETX_FLOW &&
                *p.retx_flow != *o.p.retx_flow)
                return false;
            if (p.field_mask & BTE_L2CAP_CONFIG_EXT_FLOW &&
                *p.ext_flow != *o.p.ext_flow)
                return false;
            return true;
        }

    private:
        friend class L2cap;
        void update() {
            if (p.qos) {
                m_qos.reset(new BteL2capConfigQos(*p.qos));
                p.qos = m_qos.get();
            }
            if (p.retx_flow) {
                m_retxFlow.reset(new BteL2capConfigRetxFlow(*p.retx_flow));
                p.retx_flow = m_retxFlow.get();
            }
            if (p.ext_flow) {
                m_extFlow.reset(new BteL2capConfigExtFlow(*p.ext_flow));
                p.ext_flow = m_extFlow.get();
            }
        }
        std::unique_ptr<BteL2capConfigQos> m_qos;
        std::unique_ptr<BteL2capConfigRetxFlow> m_retxFlow;
        std::unique_ptr<BteL2capConfigExtFlow> m_extFlow;

    public:
        BteL2capConfigureParams p;
    };

    struct ConfigureReply {
        uint32_t rejected_mask;
        uint32_t unknown_mask;
        ConfigureParams params;

        bool operator==(const ConfigureReply &o) const {
            return rejected_mask == o.rejected_mask &&
                unknown_mask == o.unknown_mask && params == o.params;
        }

    private:
        friend class L2cap;
        BteL2capConfigureReply toC() const {
            return { rejected_mask, unknown_mask, params.p };
        }
    };

    using ConfigureCb = std::function<void(const ConfigureReply &reply)>;
    void configure(const ConfigureParams &params,
                   const ConfigureCb &cb) {
        auto *f = new ConfigureCb(cb);
        bte_l2cap_configure(m_l2cap, &params.p,
                            &L2cap::Callbacks::configure, f);
    }

    using ConfigureRequestCb =
        std::function<void(const ConfigureParams &params)>;
    void onConfigureRequest(const ConfigureRequestCb &cb)
    {
        m_onConfigureRequest = cb;
        bte_l2cap_on_configure(m_l2cap,
                               &L2cap::Callbacks::onConfigureRequest, this);
    }

    void setConfigureReply(const ConfigureReply &reply)
    {
        BteL2capConfigureReply r = reply.toC();
        bte_l2cap_set_configure_reply(m_l2cap, &r);
    }

    bool createMessage(BufferList::Writer &writer, uint16_t size) {
        return bte_l2cap_create_message(m_l2cap, &writer.m_writer, size);
    }

    int sendMessage(BufferList &&buffer) {
        BteBuffer *b = buffer.m_buffer;
        buffer.m_buffer = nullptr;
        return bte_l2cap_send_message(m_l2cap, b);
    }

    int sendMessage(const Buffer &buffer) {
        BufferList::Writer writer;
        if (!createMessage(writer, buffer.size())) return -100;
        if (!writer.write(buffer)) return -200;
        return sendMessage(writer.end());
    }

    using MessageReceivedCb = std::function<void(BufferList::Reader &reader)>;
    void onMessageReceived(const MessageReceivedCb &cb) {
        m_onMessageReceived = cb;
        bte_l2cap_set_userdata(m_l2cap, this);
        bte_l2cap_on_message_received(
            m_l2cap, &L2cap::Callbacks::onMessageReceived);
    }

    using EchoCb = std::function<void(BufferList::Reader &reader)>;
    bool echo(const Buffer &data, const EchoCb &cb) {
        auto *f = new EchoCb(cb);
        return bte_l2cap_echo(m_l2cap, data.data(), data.size(),
                              &L2cap::Callbacks::echo, f);
    }

    using OnEchoCb = std::function<uint16_t(BufferList::Reader &reader,
                                            BufferList::Writer *writer)>;
    void onEcho(const OnEchoCb &cb) {
        m_onEcho = cb;
        bte_l2cap_on_echo(m_l2cap, &L2cap::Callbacks::onEcho, this);
    }

    void disconnect() {
        bte_l2cap_disconnect(m_l2cap);
    }

    using DisconnectedCb = std::function<void(uint8_t reason)>;
    void onDisconnected(const DisconnectedCb &cb)
    {
        m_onDisconnected = cb;
        bte_l2cap_on_disconnected(m_l2cap, &L2cap::Callbacks::onDisconnected,
                                  this);
    }

private:
    friend class ::TestL2capConfig;
    friend class ::TestL2capFixtureConnected;
    friend class ::TestL2capFixtureConfigured;
    friend class L2capServer;

    struct Callbacks {
        static void onStateChanged(
            BteL2cap *l2cap, BteL2capState state, void *d) {
            L2cap *_this = static_cast<L2cap*>(d);
            if (_this->m_onStateChanged)
                _this->m_onStateChanged(state);
        }

        static void connect(BteL2cap *l2cap,
                            const BteL2capConnectionResponse *reply,
                            void *userdata) {
            ConnectCb *cb = static_cast<ConnectCb*>(userdata);
            (*cb)(l2cap ? L2cap(bte_l2cap_ref(l2cap)) : std::optional<L2cap>{}, *reply);
            if (reply->result != BTE_L2CAP_CONN_RESP_RES_PENDING) {
                delete cb;
            }
        }

        static void configure(BteL2cap *l2cap,
                              const BteL2capConfigureReply *reply,
                              void *userdata) {
            ConfigureCb *cb = static_cast<ConfigureCb*>(userdata);
            ConfigureReply r = {
                reply->rejected_mask,
                reply->unknown_mask,
                reply->params,
            };
            (*cb)(r);
            delete cb;
        }

        static void onConfigureRequest(
            BteL2cap *l2cap, const BteL2capConfigureParams *params, void *d) {
            L2cap *_this = static_cast<L2cap*>(d);
            if (_this->m_onConfigureRequest)
                _this->m_onConfigureRequest(*params);
        }

        static void onMessageReceived(BteL2cap *l2cap, BteBufferReader *reader,
                                      void *d) {
            L2cap *_this = static_cast<L2cap*>(d);
            if (_this->m_onMessageReceived) {
                BufferList::Reader r(*reader);
                _this->m_onMessageReceived(r);
            }
        }

        static void echo(BteL2cap *l2cap, BteBufferReader *reader, void *d) {
            EchoCb *cb = static_cast<EchoCb*>(d);
            BufferList::Reader r(*reader);
            (*cb)(r);
            delete cb;
        }

        static uint16_t onEcho(BteL2cap *l2cap, BteBufferReader *reader,
                               BteBufferWriter *writer, void *d) {
            L2cap *_this = static_cast<L2cap*>(d);
            if (!_this->m_onEcho) return 0;
            BufferList::Reader r(*reader);
            BufferList::Writer w;
            if (writer) {
                w.m_writer = *writer;
            }
            uint16_t len = _this->m_onEcho(r, writer ? &w : nullptr);
            if (writer) {
                *writer = w.m_writer;
            }
            return len;
        }

        static void onDisconnected(BteL2cap *l2cap, uint8_t reason, void *d) {
            L2cap *_this = static_cast<L2cap*>(d);
            if (_this->m_onDisconnected)
                _this->m_onDisconnected(reason);
        }
    };

    BteL2cap *m_l2cap;
    ConfigureRequestCb m_onConfigureRequest;
    StateChangedCb m_onStateChanged;
    MessageReceivedCb m_onMessageReceived;
    OnEchoCb m_onEcho;
    DisconnectedCb m_onDisconnected;
};

class L2capServer
{
public:
    L2capServer(Client &client, BteL2capPsm psm):
        m_server(bte_l2cap_server_new(client.m_client, psm)) {}
    L2capServer(const L2capServer &other):
        m_server(bte_l2cap_server_ref(other.m_server)) {}
    ~L2capServer() { bte_l2cap_server_unref(m_server); }

    using ConnectedCb = std::function<void(L2cap &l2cap)>;
    void onConnected(const ConnectedCb &cb)
    {
        m_onConnected = cb;
        bte_l2cap_server_on_connected(
            m_server, &L2capServer::Callbacks::onConnected, this);
    }

private:
    struct Callbacks {
        static void onConnected(BteL2capServer *l2cap_server, BteL2cap *l2cap,
                                void *d) {
            L2capServer *_this = static_cast<L2capServer*>(d);
            L2cap l2cap_cpp(bte_l2cap_ref(l2cap));
            if (_this->m_onConnected)
                _this->m_onConnected(l2cap_cpp);
        }
    };
    BteL2capServer *m_server;
    ConnectedCb m_onConnected;
};

} // namespace Bte

inline std::ostream &operator<<(std::ostream &os,
                                const BteL2capConfigQos &a)
{
    os << "Qos{f=" << int(a.flags) <<", st=" << int(a.service_type) <<
        ", tr=" << a.token_rate << ", tbs=" << a.token_bucket_size <<
        ", pb=" << a.peak_bandwith << ", al=" << a.access_latency <<
        ", dv=" << a.delay_variation << "}";
    return os;
}

inline std::ostream &operator<<(std::ostream &os,
                                const BteL2capConfigRetxFlow &a)
{
    os << "RetxFlow{m=" << int(a.mode) <<", txw=" << int(a.tx_window_size) <<
        ", mt=" << int(a.max_transmit) << ", rt=" << a.retx_timeout <<
        ", mont=" << a.monitor_timeout <<
        ", mps=" << a.max_pdu_size << "}";
    return os;
}

inline std::ostream &operator<<(std::ostream &os,
                                const Bte::L2cap::ConfigureParams &a)
{
    os << "Params{";
    if (a.p.field_mask & BTE_L2CAP_CONFIG_MTU)
        os << " mtu=" << a.p.mtu;
    if (a.p.field_mask & BTE_L2CAP_CONFIG_FLUSH_TIMEOUT)
        os << " flush_timeout=" << a.p.flush_timeout;
    if (a.p.field_mask & BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ)
        os << " frame_check_sequence=" << int(a.p.frame_check_sequence);
    if (a.p.field_mask & BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE)
        os << " max_window_size=" << a.p.max_window_size;
    if (a.p.field_mask & BTE_L2CAP_CONFIG_QOS)
        os << " " << *a.p.qos;
    if (a.p.field_mask & BTE_L2CAP_CONFIG_RETX_FLOW)
        os << " " << *a.p.retx_flow;
    /* TODO the rest of the fields */
    os << '}';
    return os;
}

inline std::ostream &operator<<(std::ostream &os,
                                const Bte::L2cap::ConfigureReply &a)
{
    os << "ConfReply{rm=" <<
        std::hex << std::setw(8) << std::setfill('0') << a.rejected_mask <<
        " um=" <<
        std::hex << std::setw(8) << std::setfill('0') << a.unknown_mask <<
        ' ' << a.params;
    os << '}';
    return os;
}

template <typename T>
inline std::ostream &operator<<(std::ostream &os, const std::vector<T> &v)
{
    os << "vector{\n";
    for (const auto &a: v) {
        os << "  " << a << '\n';
    }
    os << '}';
    return os;
}

/* These PrintTo are needed because google test gives precedence to its
 * internal print mechanism, when printing containers:
 * https://github.com/google/googletest/issues/3458
 */
namespace Bte {
inline void PrintTo(const std::vector<L2cap::ConfigureReply> &v, std::ostream *os)
{
   *os << v;
}

inline void PrintTo(const std::vector<L2cap::ConfigureParams> &v, std::ostream *os)
{
   *os << v;
}

} /* namespace Bte */

#endif /* BTE_TESTS_BTE_L2CAP_CPP_H */
