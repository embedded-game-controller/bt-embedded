#include "backend.h"
#include "internals.h"
#include "logging.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <ogc/machine/processor.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include <ogc/system.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USB_VENDOR_NINTENDO 0x057E
#define USB_PRODUCT_WII_BT  0x0305

#define ENDPOINT_HCI_INTR 0x81
#define ENDPOINT_ACL_IN   0x82
#define ENDPOINT_ACL_OUT  0x02

#define CTRL_BUF_SIZE 660 /* From libogc */
#define ACL_BUF_SIZE  1800

#define WII_MAX_EVENTS        128
#define WII_BUFFER_INTR_COUNT 64
#define WII_BUFFER_DATA_COUNT 8

typedef enum {
    WII_EVENT_SENT,
    WII_EVENT_INTR,
    WII_EVENT_DATA,
} WiiEventType;

typedef struct wii_event_t WiiEvent;

typedef struct {
    struct wii_event_t {
        WiiEventType type;
        BteBuffer *buffer;
    } ATTRIBUTE_PACKED events[WII_MAX_EVENTS];
    int current_index;
    int missed_events;
} WiiEventQueue;

typedef struct {
    BteBuffer buffer;
    uint8_t data[CTRL_BUF_SIZE];
} WiiBufferIntr;

typedef struct {
    BteBuffer buffer;
    uint8_t data[ACL_BUF_SIZE];
} WiiBufferData;

static WiiBufferIntr *s_wii_buffer_intr;
static WiiBufferData *s_wii_buffer_data;
static WiiEventQueue s_wii_event_queue;

static int s_bt_fd = -1;
static cond_t s_event_cond = LWP_COND_NULL;
static mutex_t s_event_mutex = LWP_MUTEX_NULL;

static int read_intr();
static int read_bulk();

static void wii_buffer_free(BteBuffer *buffer)
{
    /* This should already be the case, but just to be safe */
    buffer->ref_count = 0;
}

static void *alloc_usb_buffers(int size)
{
    /* For some reason we need to allocate the buffers for the USB incoming
     * data in MEM2, otherwise some bytes in the payload get corrupted */
    void *ptr = (void *)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - size));
    if ((u32)ptr < (u32)SYS_GetArena2Lo()) return NULL;
    SYS_SetArena2Hi(ptr);
    memset(ptr, 0, size);
    return ptr;
}

static BteBuffer *wii_buffer_intr_alloc(uint16_t size)
{
    for (int i = 0; i < WII_BUFFER_INTR_COUNT; i++) {
        BteBuffer *buffer = &s_wii_buffer_intr[i].buffer;
        if (buffer->ref_count == 0) {
            buffer->ref_count = 1;
            buffer->free_func = wii_buffer_free;
            buffer->total_size = buffer->size = size;
            buffer->next = NULL;
            return buffer;
        }
    }
    return NULL;
}

static BteBuffer *wii_buffer_data_alloc(uint16_t size)
{
    for (int i = 0; i < WII_BUFFER_DATA_COUNT; i++) {
        BteBuffer *buffer = &s_wii_buffer_data[i].buffer;
        if (buffer->ref_count == 0) {
            buffer->ref_count = 1;
            buffer->free_func = wii_buffer_free;
            buffer->total_size = buffer->size = size;
            buffer->next = NULL;
            return buffer;
        }
    }
    return NULL;
}

static inline void queue_event(WiiEventType type, BteBuffer *buffer)
{
    if (s_wii_event_queue.current_index >= WII_MAX_EVENTS) {
        s_wii_event_queue.missed_events++;
        return;
    }

    WiiEvent *e = &s_wii_event_queue.events[s_wii_event_queue.current_index++];
    e->type = type;
    e->buffer = buffer;
    LWP_CondSignal(s_event_cond);
}

static s32 read_intr_cb(s32 result, void *userdata)
{
    BteBuffer *buf = userdata;

    if (result > 0) {
        bte_buffer_shrink(buf, result);
        queue_event(WII_EVENT_INTR, buf);
    } else {
        bte_buffer_unref(buf);
    }
    return read_intr();
}

static s32 read_intr()
{
    if (UNLIKELY(s_bt_fd < 0)) return -EBADF;

    u16 len = CTRL_BUF_SIZE;
    BteBuffer *buf = wii_buffer_intr_alloc(len);
    if (!buf) return -ENOMEM;

    u8 *ptr = bte_buffer_contiguous_data(buf, len);
    s32 rc = USB_ReadIntrMsgAsync(s_bt_fd, ENDPOINT_HCI_INTR, len, ptr,
                                  read_intr_cb, buf);
    return rc;
}

static s32 read_bulk_cb(s32 result, void *userdata)
{
    BteBuffer *buf = userdata;

    if (result > 0) {
        bte_buffer_shrink(buf, result);
        queue_event(WII_EVENT_DATA, buf);
    } else {
        bte_buffer_unref(buf);
    }

    return read_bulk();
}

static s32 read_bulk()
{
    if (UNLIKELY(s_bt_fd < 0)) return -EBADF;

    u16 len = ACL_BUF_SIZE;
    BteBuffer *buf = wii_buffer_data_alloc(len);
    if (!buf) return -ENOMEM;

    u8 *ptr = bte_buffer_contiguous_data(buf, len);
    s32 ret = USB_ReadBlkMsgAsync(s_bt_fd, ENDPOINT_ACL_IN, len, ptr,
                                  read_bulk_cb, buf);
    return ret;
}

static int wii_init()
{
    int rc = USB_Initialize();
    BTE_DEBUG("USB_Initialize returned %d\n", rc);
    if (rc != USB_OK) return -1;

    rc = USB_OpenDevice(USB_OH1_DEVICE_ID,
                        USB_VENDOR_NINTENDO,
                        USB_PRODUCT_WII_BT,
                        &s_bt_fd);
    BTE_DEBUG("USB_OpenDevice returned %d\n", rc);
    if (rc != USB_OK) return -1;

    LWP_CondInit(&s_event_cond);
    LWP_MutexInit(&s_event_mutex, false);

    s_wii_buffer_intr =
        alloc_usb_buffers(sizeof(WiiBufferIntr) * WII_BUFFER_INTR_COUNT);
    s_wii_buffer_data =
        alloc_usb_buffers(sizeof(WiiBufferData) * WII_BUFFER_DATA_COUNT);
    if (!s_wii_buffer_intr || !s_wii_buffer_data)
        return -ENOMEM;

    read_intr();
    read_bulk();

    return 0;
}

static int wii_handle_events(bool wait_for_events)
{
    WiiEventQueue queue;
    u32 level;

    LWP_MutexLock(s_event_mutex);
    _CPU_ISR_Disable(level);
    /* Create a copy of the queue to ensure it doesn't get modified while we
     * process it */
    memcpy(&queue, &s_wii_event_queue, sizeof(queue));
    s_wii_event_queue.current_index = 0; /* empty the queue */
    s_wii_event_queue.missed_events = 0;
    _CPU_ISR_Restore(level);
    if (wait_for_events && queue.current_index == 0) {
        /* Note that here there is a small risk of a deadlock: if an event is
         * received right at this point, the interrupt handler will run and
         * call LWP_CondSignal() before we have called LWP_CondWait()
         * ourselves, and therefore LWP_Condwait() will wait until the next
         * event happens (which could be never).
         *
         * Or maybe the wait condition is stored and can be retrieved later?
         */
        LWP_CondWait(s_event_cond, s_event_mutex);
        _CPU_ISR_Disable(level);
        memcpy(&queue, &s_wii_event_queue, sizeof(queue));
        s_wii_event_queue.current_index = 0; /* empty the queue */
        s_wii_event_queue.missed_events = 0;
        _CPU_ISR_Restore(level);
    }
    LWP_MutexUnlock(s_event_mutex);

    if (UNLIKELY(queue.missed_events > 0)) {
        BTE_WARN("%d events were not reported!", queue.missed_events);
    }

    for (int i = 0; i < queue.current_index; i++) {
        WiiEvent *event = &queue.events[i];

        if (event->type == WII_EVENT_INTR) {
            _bte_hci_dev_handle_event(event->buffer);
        } else if (event->type == WII_EVENT_DATA) {
            _bte_hci_dev_handle_data(event->buffer);
        }
        bte_buffer_unref(event->buffer);
    }

    return queue.current_index;
}

static s32 hci_send_command_cb(s32 result, void *userdata)
{
    BteBuffer *buf = userdata;
    queue_event(WII_EVENT_SENT, buf);
    return result;
}

static int wii_hci_send_command(BteBuffer *buf)
{
    BTE_DEBUG("%s fd = %d, buf %p, data %p, size=%d\n", __func__, s_bt_fd, buf,
              buf->data, buf->size);
    if (UNLIKELY(s_bt_fd < 0)) return -EBADF;

    int rc;
    if (buf->total_size == buf->size) {
        bte_buffer_ref(buf);
        rc = USB_WriteCtrlMsgAsync(s_bt_fd, 0x20, 0, 0, 0,
                                   buf->size, buf->data,
                                   hci_send_command_cb, buf);
        if (UNLIKELY(rc != USB_OK)) {
            bte_buffer_unref(buf);
            rc = -EIO;
        }
    } else {
        /* HCI commands are not big, they should all fit in a single packet */
        BTE_WARN("Sending command with size %d", buf->total_size);
        rc = -ENOMEM;
    }
    return rc;
}

static s32 hci_send_data_cb(s32 result, void *userdata)
{
    BteBuffer *buf = userdata;
    bte_buffer_unref(buf);
    return result;
}

static int wii_hci_send_data(BteBuffer *buf)
{
    if (UNLIKELY(s_bt_fd < 0)) return -EBADF;

    BteBuffer *sent_buf;

    if (buf->total_size == buf->size) {
        sent_buf = buf;
        bte_buffer_ref(buf);
    } else {
        sent_buf = bte_buffer_alloc_contiguous(buf->total_size);
        if (UNLIKELY(!sent_buf)) return -ENOMEM;

        BteBufferReader reader;
        bte_buffer_reader_init(&reader, buf);
        bte_buffer_reader_read(&reader, sent_buf->data, buf->total_size);
    }
    int rc = USB_WriteBlkMsgAsync(s_bt_fd, ENDPOINT_ACL_OUT,
                                  sent_buf->size, sent_buf->data,
                                  hci_send_data_cb, sent_buf);
    if (UNLIKELY(rc != USB_OK)) {
        bte_buffer_unref(buf);
        rc = -EIO;
    }
    return rc;
}

static int wii_deinit()
{
    USB_CloseDevice(&s_bt_fd);

    return 0;
}

const BteBackend _bte_backend = {
    .init = wii_init,

    .handle_events = wii_handle_events,

    .hci_send_command = wii_hci_send_command,
    .hci_send_data = wii_hci_send_data,

    .deinit = wii_deinit,
};
