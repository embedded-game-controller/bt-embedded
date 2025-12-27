#include "acl.h"

#include "buffer.h"
#include "client.h"
#include "hci.h"
#include "internals.h"
#include "logging.h"
#include "utils.h"

#include <stdlib.h>

#define BTE_ACL_HDR_LEN     4 /* Conn handle, flags and length */
#define BTE_ACL_HDR_POS_LEN 2

/* Packet boundary flags */
#define BTE_ACL_PB_FIRST_NO_FLUSH 0
#define BTE_ACL_PB_CONTINUATION   1
#define BTE_ACL_PB_FIRST_FLUSH    2

static BteAcl *find_acl_by_conn_handle(BteConnHandle conn_handle)
{
    BteHciDev *dev = &_bte_hci_dev;
    for (int i = 0; i < BTE_HCI_MAX_ACL; i++) {
        BteAcl *acl = dev->acls[i];
        if (acl && acl->conn_handle == conn_handle) {
            return acl;
        }
    }
    return NULL;
}

static void on_data_received(BteBuffer *buffer)
{
    uint16_t handle_and_flags = read_le16(buffer->data);
    BteConnHandle conn_handle = handle_and_flags & 0xfff;
    BteAcl *acl = find_acl_by_conn_handle(conn_handle);
    if (!acl) return;

    uint8_t packet_boundary = (handle_and_flags >> 12) & 0x3;
    if (acl->fragmented_message &&
        packet_boundary != BTE_ACL_PB_CONTINUATION) {
        /* The previous fragmented message is completed, deliver it. We should
         * not really enter this code path, because the messages are sent when
         * their last packet is received */
        BteBufferReader reader;
        bte_buffer_reader_init(&reader, acl->fragmented_message);
        bte_buffer_reader_set_header_size(&reader, BTE_ACL_HDR_LEN);
        acl->data_received_cb(acl, &reader);
        bte_buffer_unref(acl->fragmented_message);
        acl->fragmented_message = NULL;
        acl->fragmented_message_size = 0;
    }

    if (packet_boundary != BTE_ACL_PB_CONTINUATION) {
        /* The ACL header does not give us enough information to tell if the
         * message is complete: in order to do that, we need to look at the
         * L2CAP header to find out the total length of the segmented message.
         * It would be more proper if this was done in the l2cap class (and one
         * way to do that would be to invoke a "packet_received()" callback
         * here, and then have a bte_act_set_message_size() method that the
         * l2cap code would call), but since all ACL data connections have a
         * L2CAP protocol on top of them, we take a shortcut and read the
         * length from here. */
        uint16_t msg_length = read_le16(buffer->data + BTE_ACL_HDR_LEN);
        acl->reassembled_message_size = msg_length;
    }

    uint16_t packet_length = read_le16(buffer->data + BTE_ACL_HDR_POS_LEN);
    uint16_t remaining_size =
        acl->reassembled_message_size - acl->fragmented_message_size;
    if (packet_length >= remaining_size) {
        /* The message is now complete, let's deliver it */
        acl->fragmented_message =
            bte_buffer_append(acl->fragmented_message, buffer);
        BteBuffer *head = acl->fragmented_message;
        acl->fragmented_message = NULL;

        BteBufferReader reader;
        bte_buffer_reader_init(&reader, head);
        bte_buffer_reader_set_header_size(&reader, BTE_ACL_HDR_LEN);
        acl->data_received_cb(acl, &reader);

        bte_buffer_unref(head);
    } else {
        /* Not complete; queue the packet */
        if (acl->fragmented_message) {
            bte_buffer_append(acl->fragmented_message, buffer);
        } else {
            acl->fragmented_message = bte_buffer_ref(buffer);
        }
        acl->fragmented_message_size += packet_length;
    }
}

static bool bte_acl_make_public(BteAcl *acl)
{
    BteHciDev *dev = &_bte_hci_dev;
    /* Install the data callback */
    dev->data_handler_cb = on_data_received;
    /* Add the pointer to this ACL to the BteHciDev */
    for (int i = 0; i < BTE_HCI_MAX_ACL; i++) {
        if (!dev->acls[i]) {
            dev->acls[i] = acl;
            return true;
        }
    }
    return false;
}

static void bte_acl_disconnected(BteAcl *acl)
{
    BteHciDev *dev = &_bte_hci_dev;

    acl->conn_handle = BTE_CONN_HANDLE_INVALID;
    if (acl->disconnected_cb) acl->disconnected_cb(acl);

    /* Remove the pointer to this ACL from the BteHciDev */
    for (int i = 0; i < BTE_HCI_MAX_ACL; i++) {
        if (dev->acls[i] == acl) {
            dev->acls[i] = NULL;
            break;
        }
    }
}

static void bte_acl_free(BteAcl *acl)
{
    bte_acl_disconnected(acl);
    bte_client_unref(bte_hci_get_client(acl->hci));
    free(acl);
}

static bool on_disconnection_complete(
    BteHci *hci, const BteHciDisconnectionCompleteData *data, void *userdata)
{
    BteAcl *acl = find_acl_by_conn_handle(data->conn_handle);
    if (!acl) return false;

    bte_acl_disconnected(acl);
    return true;
}

static bool on_nr_of_completed_packets(
    BteHci *hci, const BteHciNrOfCompletedPacketsData *data, void *userdata)
{
    BteAcl *acl = find_acl_by_conn_handle(data->conn_handle);
    if (!acl) return false;

    acl->completed_packets_cb(acl, data->completed_packets);
    return true;
}

BteAcl *bte_acl_new(BteHci *hci, const BteBdAddr *address,
                    size_t struct_size)
{
    BteAcl *acl = malloc(struct_size);
    memset(acl, 0, struct_size);
    acl->ref_count = 1;
    acl->hci = hci;
    acl->address = *address;
    acl->conn_handle = BTE_CONN_HANDLE_INVALID;
    if (UNLIKELY(!bte_acl_make_public(acl))) {
        bte_acl_free(acl);
        return NULL;
    }

    /* We don't want to let the hci get destroyed while we are using it, so
     * let's keep a reference to the client. */
    bte_client_ref(bte_hci_get_client(hci));
    bte_hci_on_disconnection_complete(hci, on_disconnection_complete);
    bte_hci_on_nr_of_completed_packets(hci, on_nr_of_completed_packets);
    return acl;
}

BteAcl *bte_acl_get_for_address(BteHci *, const BteBdAddr *address)
{
    BteHciDev *dev = &_bte_hci_dev;
    for (int i = 0; i < BTE_HCI_MAX_ACL; i++) {
        BteAcl *acl = dev->acls[i];
        if (acl && memcmp(&acl->address, address, sizeof(*address) == 0)) {
            return acl;
        }
    }
    return NULL;
}

BteAcl *bte_acl_ref(BteAcl *acl)
{
    atomic_fetch_add(&acl->ref_count, 1);
    return acl;
}

void bte_acl_unref(BteAcl *acl)
{
    if (atomic_fetch_sub(&acl->ref_count, 1) == 1) {
        bte_acl_free(acl);
    }
}

static void connect_status_cb(BteHci *hci, const BteHciReply *reply,
                              void *userdata)
{
    BteAcl *acl = userdata;

    if (reply->status != 0) {
        bte_acl_ref(acl);
        /* The operation failed. Notify the client */
        acl->connected_cb(acl, reply->status);
        bte_acl_unref(acl);
    }
}

static void connect_cb(BteHci *hci, const BteHciCreateConnectionReply *reply,
                       void *userdata)
{
    BteAcl *acl = bte_acl_ref(userdata);
    if (reply->status == 0) {
        acl->conn_handle = reply->conn_handle;
        acl->encryption_mode = reply->encryption_mode;
    }
    acl->connected_cb(acl, reply->status);
    bte_acl_unref(acl);
}

void bte_acl_connect(BteAcl *acl, const BteHciConnectParams *params)
{
    bte_hci_create_connection(acl->hci, &acl->address, params,
                              connect_status_cb, connect_cb, acl);
}

void bte_acl_disconnect(BteAcl *acl)
{
    if (acl->conn_handle == BTE_CONN_HANDLE_INVALID) {
        /* already disconnected or never connected */
        return;
    }
    bte_hci_disconnect(acl->hci, acl->conn_handle,
                       HCI_OTHER_END_TERMINATED_CONN_USER_ENDED,
                       /* No callback, as there's nothing we should do in case
                        * of error */
                       NULL, NULL);
}

bool bte_acl_create_message(BteAcl *acl, BteBufferWriter *writer, uint16_t size,
                            uint8_t broadcast)
{
    uint16_t packet_size = bte_hci_get_acl_mtu(acl->hci);
    uint16_t req_size = _bte_compute_fragmented_size(size, packet_size,
                                                     BTE_ACL_HDR_LEN);
    BteBuffer *head = bte_buffer_alloc(req_size, packet_size);
    if (!head) return false;

    /* Write the header on all packets */
    uint8_t packet_boundary = BTE_ACL_PB_FIRST_FLUSH;
    uint16_t packet_max_size = packet_size - BTE_ACL_HDR_LEN;
    for (BteBuffer *buffer = head; buffer != NULL; buffer = buffer->next) {
        uint16_t conn_and_flags = acl->conn_handle;
        conn_and_flags |= (packet_boundary << 12);
        conn_and_flags |= (broadcast << 14);
        write_le16(conn_and_flags, buffer->data);
        uint16_t packet_data_size = size;
        if (packet_data_size > packet_max_size) {
            packet_data_size = packet_max_size;
        }
        write_le16(packet_data_size, buffer->data + BTE_ACL_HDR_POS_LEN);
        packet_boundary = BTE_ACL_PB_CONTINUATION;
        size -= packet_data_size;
    }

    bte_buffer_writer_init(writer, head);
    bte_buffer_writer_set_header_size(writer, BTE_ACL_HDR_LEN);
    return true;
}

int bte_acl_send_message(BteAcl *acl, BteBuffer *buffer)
{
    BteHciDev *dev = &_bte_hci_dev;

    dev->outgoing_acl_packets =
        bte_buffer_append(dev->outgoing_acl_packets, buffer);
    int rc = _bte_hci_send_queued_data();
    if (UNLIKELY(rc < 0)) return rc;

    /* Check if our buffer is still in the queue */
    int queued_packets = 0;
    for (BteBuffer *b = dev->outgoing_acl_packets; b != NULL; b = b->next) {
        queued_packets++;
    }
    int our_packets = 0;
    for (BteBuffer *b = buffer; b != NULL; b = b->next) our_packets++;
    bte_buffer_unref(buffer);
    return queued_packets >= our_packets ? 0 : (our_packets - queued_packets);
}
