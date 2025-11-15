#ifndef BTE_TESTS_HELPERS_H
#define BTE_TESTS_HELPERS_H

#include "mock_backend.h"
#include "type_utils.h"

#include "bt-embedded/client.h"
#include "bt-embedded/bte.h"
#include "bt-embedded/hci.h"

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

#endif /* BTE_TESTS_HELPERS_H */
