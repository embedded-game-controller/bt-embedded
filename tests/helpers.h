#ifndef BTE_TESTS_HELPERS_H
#define BTE_TESTS_HELPERS_H

#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"

#include <type_traits>

template <typename ReplyType>
class GetterInvoker {
public:
    typedef void (*ReplyCb)(BteHci *hci,
                            const ReplyType *reply,
                            void *userdata);
    using InvokerCb = std::function<void(BteHci *, ReplyCb replyCb)>;
    using ReplyParams = std::tuple<BteHci *, ReplyType, void*>;
    GetterInvoker(const InvokerCb &invoker,
                  const Buffer &eventData) {
        BteClient *client = bte_client_new();
        m_hci = bte_hci_get(client);

        void *expectedUserdata = (void*)this;
        bte_client_set_userdata(client, expectedUserdata);

        invoker(m_hci, &GetterInvoker::replyCb);

        m_backend.sendEvent(eventData);
        bte_handle_events();
    }

    ~GetterInvoker() {
        BteClient *client = bte_hci_get_client(m_hci);
        bte_client_unref(client);
        m_hci = nullptr;
    }

    Buffer sentCommand() const { return m_backend.lastCommand(); }

    const ReplyType &receivedReply() const {
        static const ReplyType invalidReply = {0};
        return m_replies.empty() ? invalidReply : std::get<1>(m_replies.front());
    }

    static void replyCb(BteHci *hci, const ReplyType *reply, void *userdata) {
        auto _this = static_cast<GetterInvoker<ReplyType> *>(userdata);
        _this->m_replies.push_back({hci, *reply, userdata});
    }

private:
    MockBackend m_backend;
    BteHci *m_hci;
    std::vector<ReplyParams> m_replies;
};

template <typename ReplyType>
class AsyncCommandInvoker {
public:
    using BteType = typename ReplyType::BteType;
    typedef void (*ReplyCb)(BteHci *hci, const BteType *reply, void *userdata);
    using InvokerCb = std::function<void(BteHci *,
                                         BteHciDoneCb statusCb,
                                         ReplyCb replyCb)>;
    using ReplyParams = std::tuple<BteHci *, ReplyType, void*>;
    using StatusParams = std::tuple<BteHci *, BteHciReply, void*>;
    AsyncCommandInvoker(const InvokerCb &invoker,
                        const Buffer &eventData) {
        BteClient *client = bte_client_new();
        m_hci = bte_hci_get(client);

        void *expectedUserdata = (void*)this;
        bte_client_set_userdata(client, expectedUserdata);

        invoker(m_hci,
                &AsyncCommandInvoker::statusCb,
                &AsyncCommandInvoker::replyCb);

        m_backend.sendEvent(eventData);
        bte_handle_events();
    }

    ~AsyncCommandInvoker() {
        BteClient *client = bte_hci_get_client(m_hci);
        bte_client_unref(client);
        m_hci = nullptr;
    }

    MockBackend &backend() { return m_backend; }
    Buffer sentCommand() const { return m_backend.lastCommand(); }
    std::vector<BteHciReply> receivedStatuses() const {
        std::vector<BteHciReply> ret;
        for (const auto &status: m_statuses) {
            ret.push_back(std::get<1>(status));
        }
        return ret;
    }

    size_t replyCount() const { return m_replies.size(); }
    const std::vector<ReplyType> receivedReplies() const {
        std::vector<ReplyType> ret;
        for (const auto &reply: m_replies) {
            ret.push_back(std::get<1>(reply));
        }
        return ret;
    }

    const ReplyType &receivedReply() const {
        static const ReplyType invalidReply;
        return m_replies.empty() ? invalidReply : std::get<1>(m_replies.back());
    }

    static void statusCb(BteHci *hci, const BteHciReply *reply,
                         void *userdata) {
        auto _this = static_cast<AsyncCommandInvoker<ReplyType> *>(userdata);
        _this->m_statuses.push_back({hci, *reply, userdata});
    }
    static void replyCb(BteHci *hci, const BteType *reply, void *userdata) {
        auto _this = static_cast<AsyncCommandInvoker<ReplyType> *>(userdata);
        _this->m_replies.push_back({hci, *reply, userdata});
    }

private:
    MockBackend m_backend;
    BteHci *m_hci;
    std::vector<StatusParams> m_statuses;
    std::vector<ReplyParams> m_replies;
};

#endif /* BTE_TESTS_HELPERS_H */
