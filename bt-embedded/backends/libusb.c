#include "libusb.h"

#include "backend.h"
#include "internals.h"
#include "logging.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define ENDPOINT_HCI_INTR 0x81
#define ENDPOINT_ACL_IN   0x82
#define ENDPOINT_ACL_OUT  0x02

/* The max size of an HCI event is 255 (payload) + 3 (header); here we round it
 * up a bit */
#define CTRL_BUF_SIZE 264
/* Typical MTU is 339; we round it up */
#define ACL_BUF_SIZE  352

#define LIBUSB_MAX_EVENTS        128
#define LIBUSB_BUFFER_INTR_COUNT 16
#define LIBUSB_BUFFER_DATA_COUNT 64

#define TO_DEF 5000 /* 5 seconds */

/* A list of USB BT adaptors */
static const struct {
    uint16_t vid;
    uint16_t pid;
} s_known_devices[] = {
    { 0x0a5c, 0x21ec }, /* Broadcom Corp. BCM20702A0 Bluetooth 4.0 */
    { 0, 0 },
};

static bool s_external_context = false;
static libusb_context *s_context = NULL;
static libusb_device_handle *s_handle = NULL;
static int s_num_events = 0;

static int read_intr();
static int read_bulk();

static void read_bulk_cb(struct libusb_transfer *transfer)
{
    BteBuffer *buf = transfer->user_data;

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        bte_buffer_shrink(buf, transfer->actual_length);
        _bte_hci_dev_handle_data(buf);
        s_num_events++;
    }
    libusb_free_transfer(transfer);
    bte_buffer_unref(buf);
    read_bulk();
}

static int read_bulk()
{
    if (UNLIKELY(!s_handle)) return -EBADF;

    struct libusb_transfer *transfer = libusb_alloc_transfer(0);

    uint16_t len = ACL_BUF_SIZE;
    BteBuffer *buf = bte_buffer_alloc_contiguous(len);
    if (!buf) {
        return -ENOMEM;
    }

    uint8_t *ptr = bte_buffer_contiguous_data(buf, len);
    libusb_fill_bulk_transfer(transfer, s_handle, ENDPOINT_ACL_IN, ptr, len,
                              read_bulk_cb, buf, TO_DEF);
    int rc = libusb_submit_transfer(transfer);
    if (UNLIKELY(rc != 0)) {
        BTE_DEBUG("libusb_submit_transfer failed %s", libusb_strerror(rc));
        return -1;
    }

    return rc;
}

static void read_intr_cb(struct libusb_transfer *transfer)
{
    BteBuffer *buf = transfer->user_data;

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        bte_buffer_shrink(buf, transfer->actual_length);
        _bte_hci_dev_handle_event(buf);
        s_num_events++;
    }
    libusb_free_transfer(transfer);
    bte_buffer_unref(buf);
    read_intr();
}

static int read_intr()
{
    if (UNLIKELY(!s_handle)) return -EBADF;

    struct libusb_transfer *transfer = libusb_alloc_transfer(0);

    uint16_t len = CTRL_BUF_SIZE;
    BteBuffer *buf = bte_buffer_alloc_contiguous(len);
    if (!buf) {
        return -ENOMEM;
    }

    uint8_t *ptr = bte_buffer_contiguous_data(buf, len);
    libusb_fill_interrupt_transfer(transfer, s_handle, ENDPOINT_HCI_INTR,
                                   ptr, len, read_intr_cb, buf, TO_DEF);
    int rc = libusb_submit_transfer(transfer);
    if (UNLIKELY(rc != 0)) {
        BTE_DEBUG("libusb_submit_transfer failed %s", libusb_strerror(rc));
        return -1;
    }

    return rc;
}

static bool is_bluetooth_adaptor(const struct libusb_device_descriptor *desc)
{
    const uint8_t subclass = 0x01;
    const uint8_t proto_bluetooth = 0x01;
    if (desc->bDeviceClass == LIBUSB_CLASS_WIRELESS &&
        desc->bDeviceSubClass == subclass &&
        desc->bDeviceProtocol == proto_bluetooth) {
        return true;
    }

    /* Otherwise, look it up in our list */
    for (int i = 0; s_known_devices[i].vid != 0; i++) {
        if (s_known_devices[i].vid == desc->idVendor &&
            s_known_devices[i].pid == desc->idProduct) {
            return true;
        }
    }

    return false;
}

static int usb_backend_init()
{
    int rc;

    if (!s_external_context) {
        rc = libusb_init_context(&s_context, NULL, 0);
        if (UNLIKELY(rc != 0)) return -1;
    }

    libusb_device **devices;
    int n_devices = libusb_get_device_list(s_context, &devices);
    for (int i = 0; i < n_devices; i++) {
        libusb_device *d = devices[i];

        struct libusb_device_descriptor desc;
        rc = libusb_get_device_descriptor(d, &desc);
        if (UNLIKELY(rc != 0)) {
            BTE_WARN("Failed to read USB descriptor: %s", libusb_strerror(rc));
            continue;
        }


        if (is_bluetooth_adaptor(&desc)) {
            BTE_DEBUG("Trying device %04x:%04x", desc.idVendor, desc.idProduct);
            rc = libusb_open(d, &s_handle);
            if (UNLIKELY(rc != 0)) {
                BTE_WARN("Failed to open USB device: %s", libusb_strerror(rc));
                continue;
            }

            break;
        }
    }

    libusb_free_device_list(devices, true);

    if (UNLIKELY(!s_handle)) {
        BTE_WARN("No USB bluetooth adapters found");
        return -1;
    }

    const uint8_t interface = 0x00;
    rc = libusb_set_auto_detach_kernel_driver(s_handle, 1);
    if (UNLIKELY(rc != 0)) {
        rc = libusb_detach_kernel_driver(s_handle, interface);
        if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_NOT_FOUND &&
            rc != LIBUSB_ERROR_NOT_SUPPORTED)
        {
            BTE_WARN("Failed to detach kernel driver: %s", libusb_strerror(rc));
            return -1;
        }
    }

    rc = libusb_claim_interface(s_handle, interface);
    if (UNLIKELY(rc != 0)) {
        BTE_WARN("Failed to claim interface: %s", libusb_strerror(rc));
        return -1;
    }

    read_intr();
    read_bulk();

    return 0;
}

static int usb_backend_handle_events(bool wait_for_events, uint32_t timeout_us)
{
    struct timeval tv;
    if (wait_for_events) {
        tv.tv_sec = timeout_us / 1000000;
        tv.tv_usec = timeout_us % 1000000;
    }
    s_num_events = 0;
    int rc = libusb_handle_events_timeout_completed(s_context,
                                                    wait_for_events ? &tv : NULL,
                                                    &s_num_events);
    if (UNLIKELY(rc != 0)) {
        BTE_WARN("Failed to read events: %s", libusb_strerror(rc));
        return -EAGAIN;
    }

    return s_num_events;
}

static void hci_send_command_cb(struct libusb_transfer *transfer)
{
    free(transfer->buffer);
    libusb_free_transfer(transfer);
}

static int usb_backend_hci_send_command(BteBuffer *buf)
{
    if (UNLIKELY(!s_handle)) return -EBADF;

    int rc;
    if (buf->total_size == buf->size) {
        struct libusb_transfer *transfer = libusb_alloc_transfer(0);

        uint8_t *buffer = malloc(LIBUSB_CONTROL_SETUP_SIZE + buf->size);
        libusb_fill_control_setup(buffer, LIBUSB_REQUEST_TYPE_CLASS, 0, 0, 0,
                                  buf->size);
        memcpy(buffer + LIBUSB_CONTROL_SETUP_SIZE, buf->data, buf->size);
        libusb_fill_control_transfer(transfer, s_handle, buffer,
                                     hci_send_command_cb, NULL, TO_DEF);
        rc = libusb_submit_transfer(transfer);
        if (UNLIKELY(rc != 0)) {
            rc = -EIO;
        }
    } else {
        /* HCI commands are not big, they should all fit in a single packet */
        BTE_WARN("Sending command with size %d", buf->total_size);
        rc = -ENOMEM;
    }
    return rc;
}

static void hci_send_data_cb(struct libusb_transfer *transfer)
{
    BteBuffer *buf = transfer->user_data;
    bte_buffer_unref(buf);
    libusb_free_transfer(transfer);
}

static int usb_backend_hci_send_data(BteBuffer *buf)
{
    if (UNLIKELY(!s_handle)) return -EBADF;

    bte_buffer_ref(buf);

    struct libusb_transfer *transfer = libusb_alloc_transfer(0);

    libusb_fill_bulk_transfer(transfer, s_handle, ENDPOINT_ACL_OUT,
                              buf->data, buf->size,
                              hci_send_data_cb, buf, TO_DEF);
    int rc = libusb_submit_transfer(transfer);
    if (UNLIKELY(rc != 0)) {
        bte_buffer_unref(buf);
        rc = -EIO;
    }
    return rc;
}

static int usb_backend_deinit()
{
    BTE_DEBUG("");
    libusb_close(s_handle);
    if (!s_external_context) {
        libusb_exit(s_context);
    }
    return 0;
}

const BteBackend _bte_backend = {
    .init = usb_backend_init,

    .handle_events = usb_backend_handle_events,

    .hci_send_command = usb_backend_hci_send_command,
    .hci_send_data = usb_backend_hci_send_data,

    .deinit = usb_backend_deinit,
};

void bte_backend_libusb_set_context(libusb_context *context)
{
    s_external_context = true;
    s_context = context;
}
