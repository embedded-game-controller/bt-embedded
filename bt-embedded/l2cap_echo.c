#include "l2cap_priv.h"

#include "buffer.h"
#include "internals.h"
#include "l2cap_proto.h"
#include "logging.h"

#include <stdlib.h>

bool acl_l2cap_handle_echo_req(BteAclL2cap *acl_l2cap, uint8_t id,
                               BteBufferReader *reader, uint16_t req_len)
{
    uint16_t rsp_len = 0;

    BteL2cap *l2cap = NULL;
    for (int i = 0; i < BTE_ACL_MAX_CLIENTS; i++) {
        BteL2cap *l = acl_l2cap->clients[i];
        if (l && l->echo_cb) {
            /* Create a copy of the reader, since the callback will likely
             * modify it */
            BteBufferReader r = *reader;
            rsp_len = l->echo_cb(l, &r, NULL, l->echo_userdata);
            if (rsp_len > 0) {
                l2cap = l;
                break;
            }
        }
    }

    BteBufferWriter writer;
    bool ok = acl_l2cap_create_cmd(acl_l2cap, &writer, L2CAP_SIGNAL_ECHO_RSP,
                                   id, rsp_len);
    if (UNLIKELY(!ok)) return false;

    if (l2cap) {
        l2cap->echo_cb(l2cap, reader, &writer, l2cap->echo_userdata);
    }

    bte_acl_send_message(&acl_l2cap->acl, bte_buffer_writer_end(&writer));
    return true;
}

void bte_l2cap_on_echo(
    BteL2cap *l2cap, BteL2capOnEchoCb callback, void *userdata)
{
    l2cap->echo_cb = callback;
    l2cap->echo_userdata = userdata;
}
