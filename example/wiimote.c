#include "terminal.h"

#include <ogc/conf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include "wiiuse_internal.h"

#include "bt-embedded/bte.h"
#include "bt-embedded/client.h"
#include "bt-embedded/hci.h"
#include "bt-embedded/l2cap.h"
#include "bt-embedded/l2cap_server.h"
#include "bt-embedded/services/hid.h"

#ifndef __wii__
#  include <endian.h>
#else
#  include <sys/endian.h>
#endif

#define read_be16(ptr) be16toh(*(uint16_t *)(ptr))
#define read_be32(ptr) be32toh(*(uint32_t *)(ptr))

#define write_be16(n, ptr) *(uint16_t *)(ptr) = htobe16(n)
#define write_be32(n, ptr) *(uint32_t *)(ptr) = htobe32(n)

/* Error codes from bte/bte.h */
#define ERR_OK                      0
#define ERR_MEM                     -1
#define ERR_BUF                     -2
#define ERR_ABRT                    -3
#define ERR_RST                     -4
#define ERR_CLSD                    -5
#define ERR_CONN                    -6
#define ERR_VAL                     -7
#define ERR_ARG                     -8
#define ERR_RTE                     -9
#define ERR_USE                     -10
#define ERR_IF                      -11
#define ERR_PKTSIZE                 -17

#define MAX_INQUIRY_RESPONSES 8
#define CONF_PAD_MAX_ACTIVE 4

#define WIIUSE_DEBUG(fmt, ...)  fprintf(stderr, "[DEBUG] %s:%i: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define WIIUSE_WARNING(fmt, ...) fprintf(stderr, "[WARNING] " fmt "\n",  ##__VA_ARGS__)
#define WIIUSE_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n",  ##__VA_ARGS__)

#define BD_ADDR_FROM_CONF(bdaddr, b) do { \
    (bdaddr)->bytes[0] = b[5]; \
    (bdaddr)->bytes[1] = b[4]; \
    (bdaddr)->bytes[2] = b[3]; \
    (bdaddr)->bytes[3] = b[2]; \
    (bdaddr)->bytes[4] = b[1]; \
    (bdaddr)->bytes[5] = b[0]; } while(0)

#define BD_ADDR_TO_CONF(b, bdaddr) do { \
    b[0] = (bdaddr)->bytes[5]; \
    b[1] = (bdaddr)->bytes[4]; \
    b[2] = (bdaddr)->bytes[3]; \
    b[3] = (bdaddr)->bytes[2]; \
    b[4] = (bdaddr)->bytes[1]; \
    b[5] = (bdaddr)->bytes[0]; } while(0)

#define BD_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define BD_ADDR_DATA(b) \
    (b)->bytes[5], (b)->bytes[4], (b)->bytes[3], \
    (b)->bytes[2], (b)->bytes[1], (b)->bytes[0]

typedef struct {
    BteBdAddr address;
    BteL2cap *hid_ctrl;
    BteL2cap *hid_intr;
    int unid; /* TODO: figure out if needed */
    enum {
        STATE_HANDSHAKE_LEDS_OFF,
        STATE_HANDSHAKE_READ_EXPANSION,
        STATE_HANDSHAKE_READ_CALIBRATION,
        STATE_HANDSHAKE_COMPLETE,
    } state;
    uint16_t btn_pressed;
    uint16_t btn_held;
    uint16_t btn_released;
    uint8_t battery_level;
    uint8_t speaker_volume;
    bool continuous : 1;
    bool rumble : 1;
    bool battery_critical : 1;
    bool ir_enabled : 1;
    bool accel_enabled : 1;
    bool speaker_enabled : 1;
    unsigned exp_state : 3;
    bool is_wiiu_pro : 1;
    unsigned ir_sensor_level : 3;
    uint32_t last_read_offset;
    uint16_t last_read_size;
    uint16_t last_read_cursor;
    uint8_t *last_read_data; /* NULL if data is less than 16 bytes */
    struct accel_t accel_calibration;
    struct expansion_t exp;
} WpadDevice;

enum {
    EXP_STATE_DISCONNECTED,
    EXP_STATE_ENABLE_1,
    EXP_STATE_ENABLE_2,
    EXP_STATE_READ_CALIBRATION,
    EXP_STATE_SPECIFIC,
    EXP_STATE_READY,
    EXP_STATE_FAILED,
} ExpState;

static struct {
    int num_responses;
    int num_names_queried;
    BteHciInquiryResponse responses[MAX_INQUIRY_RESPONSES];
} *s_inquiry_responses = NULL;

static BteClient *s_client;
static BteL2capServer *s_l2cap_server_hid_ctrl;
static BteL2capServer *s_l2cap_server_hid_intr;
static enum {
    WPAD_PAIR_MODE_NORMAL,
    WPAD_PAIR_MODE_TEMPORARY,
} s_pair_mode;
static BteBdAddr s_local_address;

static BtePacketType s_packet_types = 0;
static char s_nintendo_rvl[] = "Nintendo RVL-";
static const uint8_t s_speaker_defconf[7] = {
    0x00, 0x00, 0xD0, 0x07, 0x40, 0x0C, 0x0E
};

static WpadDevice s_devices[WPAD_MAX_DEVICES];
static vu32 __wpads_used = 0;
static conf_pads __wpad_devs = {0};
static conf_pad_guests __wpad_guests = {0};

static void inquiry_status_cb(BteHci *hci, const BteHciReply *reply, void *)
{
    printf("Inquiry issued, status = %d\n", reply->status);
}

static inline void print_hex(const uint8_t *b, int n, char sep)
{
    for (int i = 0; i < n; i++) {
        printf("%02x%c", b[i], (i == n - 1) ? ' ' : sep);
    }
}

static inline void print_addr(const BteBdAddr *address)
{
    print_hex(address->bytes, 6, ':');
}


static bool address_is_new(const BteBdAddr *address)
{
    for (int i = 0; i < s_inquiry_responses->num_responses; i++) {
        if (memcmp(address, &s_inquiry_responses->responses[i].address, 6) == 0)
            return false;
    }
    return true;
}

static inline bool bd_address_is_equal(const BteBdAddr *dest,
                                       const BteBdAddr *src)
{
    return memcmp(dest, src, sizeof(BteBdAddr)) == 0;
}

static inline void bd_address_copy(BteBdAddr *dest, const BteBdAddr *src)
{
    *dest = *src;
}

static void query_name_next(BteHci *hci);

typedef void (*WpadDeviceCmdCb)(WpadDevice *device, uint8_t *data, int len);

static bool device_io_write(WpadDevice *device, uint8_t *data, int len)
{
    if (!device->hid_intr) return false;

    BteBufferWriter writer;
    bool ok = bte_l2cap_create_message(device->hid_intr, &writer, len + 1);
    if (!ok) return false;

    uint8_t *buf = bte_buffer_writer_ptr_n(&writer, len + 1);
    buf[0] = BTE_HID_TRANS_DATA | BTE_HID_REP_TYPE_OUTPUT;
    memcpy(buf + 1, data, len);
    int rc = bte_l2cap_send_message(device->hid_intr,
                                    bte_buffer_writer_end(&writer));
    return rc >= 0;
}

static bool device_send_command(WpadDevice *device, uint8_t *data, int len)
{
    if (device->rumble) data[1] |= 0x01;
    return device_io_write(device, data, len);
}

static bool device_sendcmd(WpadDevice *device, uint8_t report_type,
                           uint8_t *data, int len)
{
    /* TODO: optimize this, avoid memcpy */
    uint8_t cmd[48];
    cmd[0] = report_type;
    memcpy(cmd + 1, data, len);
    if (report_type != WM_CMD_READ_DATA && report_type != WM_CMD_CTRL_STATUS) {
        cmd[1] |= 0x02; /* ACK requested */
    }
    WIIUSE_DEBUG("Pushing command: %02x %02x", cmd[0], cmd[1]);
    return device_send_command(device, cmd, len + 1);
}

static bool device_request_status(WpadDevice *device)
{
    uint8_t buf = 0;
    return device_sendcmd(device, WM_CMD_CTRL_STATUS, &buf, 1);
}

static bool device_read_data(WpadDevice *device, uint32_t offset, uint16_t size)
{
    uint8_t msg[1 + 4 + 2];

    /* Only one read at a time! */
    if (device->last_read_size) return false;

    msg[0] = WM_CMD_READ_DATA;
    write_be32(offset, msg + 1);
    write_be16(size, msg + 1 + 4);
    bool ok = device_send_command(device, msg, sizeof(msg));
    if (ok) {
        device->last_read_offset = offset;
        device->last_read_size = size;
        device->last_read_cursor = 0;
        device->last_read_data = size > 16 ? malloc(size) : NULL;
    }
    return ok;
}

static bool device_write_data(WpadDevice *device, uint32_t offset,
                              const void *data, uint8_t size)
{
    uint8_t msg[1 + 4 + 1 + 16];

    if (size > 16) return false;

    msg[0] = WM_CMD_WRITE_DATA;
    uint8_t *ptr = msg + 1;
    write_be32(offset, ptr);
    ptr += 4;
    ptr[0] = size;
    ptr++;
    memcpy(ptr, data, size);
    ptr += size;
    memset(ptr, 0, 16 - size);
    return device_send_command(device, msg, sizeof(msg));
}

static uint8_t device_set_report_type(WpadDevice *device)
{
    if (device->state != STATE_HANDSHAKE_COMPLETE) return 0;

    uint8_t buf[2];
    buf[0] = device->continuous ? 0x04 : 0x00;

    bool motion = device->accel_enabled || device->ir_enabled;
    bool exp = device->exp_state == EXP_STATE_READY;
    bool ir = device->ir_enabled;

    if (motion && ir && exp) buf[1] = WM_RPT_BTN_ACC_IR_EXP;
    else if (motion && exp)  buf[1] = WM_RPT_BTN_ACC_EXP;
    else if (motion && ir)   buf[1] = WM_RPT_BTN_ACC_IR;
    else if (ir && exp)      buf[1] = WM_RPT_BTN_IR_EXP;
    else if (ir)             buf[1] = WM_RPT_BTN_ACC_IR;
    else if (exp)            buf[1] = WM_RPT_BTN_EXP;
    else if (motion)         buf[1] = WM_RPT_BTN_ACC;
    else                     buf[1] = WM_RPT_BTN;

    WIIUSE_DEBUG("Setting report type: 0x%02x", buf[1]);

    device_sendcmd(device, WM_CMD_REPORT_TYPE, buf, 2);
    return buf[1];
}

static bool device_set_leds(WpadDevice *device, int leds)
{
    uint8_t buf;
    leds &= 0xf0;
    buf = leds;
    return device_sendcmd(device, WM_CMD_LED, &buf, 1);
}

/**
 *	@brief	Get the IR sensitivity settings.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param block1	[out] Pointer to where block1 will be set.
 *	@param block2	[out] Pointer to where block2 will be set.
 *
 *	@return Returns the sensitivity level.
 */
static int get_ir_sens(WpadDevice *device,
                       const uint8_t **block1, const uint8_t **block2) {
    switch (device->ir_sensor_level) {
    case 1:
		*block1 = WM_IR_BLOCK1_LEVEL1;
		*block2 = WM_IR_BLOCK2_LEVEL1;
		return 1;
    case 2:
		*block1 = WM_IR_BLOCK1_LEVEL2;
		*block2 = WM_IR_BLOCK2_LEVEL2;
		return 2;
    case 3:
		*block1 = WM_IR_BLOCK1_LEVEL3;
		*block2 = WM_IR_BLOCK2_LEVEL3;
		return 3;
    case 4:
		*block1 = WM_IR_BLOCK1_LEVEL4;
		*block2 = WM_IR_BLOCK2_LEVEL4;
		return 4;
    case 5:
		*block1 = WM_IR_BLOCK1_LEVEL5;
		*block2 = WM_IR_BLOCK2_LEVEL5;
		return 5;
	}

	*block1 = NULL;
	*block2 = NULL;
	return 0;
}

static void device_set_ir(WpadDevice *device, bool enable)
{
    /*
     *  Wait for the handshake to finish first.
     *  When it handshake finishes and sees that
     *  IR is enabled, it will call this function
     *  again to actually enable IR.
     */
    if (device->state != STATE_HANDSHAKE_COMPLETE) {
        WIIUSE_DEBUG("Tried to enable IR, will wait until handshake finishes.");
        device->ir_enabled = enable;
        return;
    }

    /*
     *  Check to make sure a sensitivity setting is selected.
     */
    const uint8_t *block1, *block2;
    int ir_level = get_ir_sens(device, &block1, &block2);
    if (!ir_level) {
        WIIUSE_ERROR("No IR sensitivity setting selected.");
        return;
    }

    if (enable == device->ir_enabled || device->is_wiiu_pro) {
        goto done;
    }

    uint8_t buf = (enable ? 0x04 : 0x00);
    device_sendcmd(device, WM_CMD_IR, &buf, 1);
    device_sendcmd(device, WM_CMD_IR_2, &buf, 1);

    if (!enable) {
        WIIUSE_DEBUG("Disabled IR cameras for wiimote id %i.", device->unid);
        goto done;
    }

    /* enable IR, set sensitivity */
    buf = 0x08;
    device_write_data(device, WM_REG_IR, &buf, 1);

    device_write_data(device, WM_REG_IR_BLOCK1, block1, 9);
    device_write_data(device, WM_REG_IR_BLOCK2, block2, 2);

    buf = device->exp_state == EXP_STATE_READY ?
        WM_IR_TYPE_BASIC : WM_IR_TYPE_EXTENDED;
    device_write_data(device, WM_REG_IR_MODENUM, &buf, 1);

done:
    device_request_status(device);
}

void device_set_ir_mode(WpadDevice *device)
{
    if (!device->ir_enabled) return;

	uint8_t buf = device->exp_state == EXP_STATE_READY ?
        WM_IR_TYPE_BASIC : WM_IR_TYPE_EXTENDED;
	device_write_data(device, WM_REG_IR_MODENUM, &buf, 1);
}

static void device_set_speaker(WpadDevice *device, bool enable)
{
	if (device->state != WIIMOTE_STATE_HANDSHAKE_COMPLETE) {
		WIIUSE_DEBUG("Tried to enable speaker, will wait until handshake finishes.");
        device->speaker_enabled = enable;
		return;
	}

	if (enable == device->speaker_enabled) {
        /* Nothing to do */
        device_request_status(device);
        return;
    }

	uint8_t buf = 0x04;
	device_sendcmd(device, WM_CMD_SPEAKER_MUTE, &buf, 1);

	if (!enable) {
		WIIUSE_DEBUG("Disabled speaker for wiimote id %i.", device->unid);

		buf = 0x01;
		device_write_data(device, WM_REG_SPEAKER_REG1, &buf, 1);

		buf = 0x00;
		device_write_data(device, WM_REG_SPEAKER_REG3, &buf, 1);

		buf = 0x00;
		device_sendcmd(device, WM_CMD_SPEAKER_ENABLE, &buf, 1);

        device_request_status(device);
		return;
	}

	buf = 0x04;
	device_sendcmd(device, WM_CMD_SPEAKER_ENABLE, &buf, 1);

	buf = 0x01;
	device_write_data(device, WM_REG_SPEAKER_REG3, &buf, 1);

	buf = 0x08;
	device_write_data(device, WM_REG_SPEAKER_REG1, &buf, 1);

	uint8_t conf[7];
	memcpy(conf, s_speaker_defconf, 7);
	conf[4] = device->speaker_volume;
	device_write_data(device, WM_REG_SPEAKER_BLOCK, conf, 7);

	buf = 0x01;
	device_write_data(device, WM_REG_SPEAKER_REG2, &buf, 1);

	buf = 0x00;
	device_sendcmd(device, WM_CMD_SPEAKER_MUTE, &buf, 1);

	device_request_status(device);
}

static inline int wpad_device_get_slot(const WpadDevice *device)
{
    return device ? (device - s_devices) : -1;
}

static WpadDevice *wpad_device_from_addr(const BteBdAddr *address)
{
    for (int i = 0; i < WPAD_MAX_DEVICES; i++) {
        WpadDevice *device = &s_devices[i];
        if (bd_address_is_equal(address, &device->address)) {
            return device;
        }
    }
    return NULL;
}

static bool wpad_device_is_pairing(const BteBdAddr *address)
{
    WpadDevice *device = wpad_device_from_addr(address);
    return device ? (device->hid_ctrl == NULL) : false;
}

static int GetActiveSlot(const BteBdAddr *pad_addr)
{
    int slot = CONF_PAD_MAX_ACTIVE;
    BteBdAddr bdaddr;

    for (int i = 0; i < CONF_PAD_MAX_ACTIVE; i++) {
        BD_ADDR_FROM_CONF(&bdaddr, __wpad_devs.active[i].bdaddr);
        if (bd_address_is_equal(pad_addr, &bdaddr)) {
            slot = i;
            break;
        }
    }

    return slot;
}

static void device_handshake_expansion_calibrate(
    WpadDevice *device, const uint8_t *data, uint16_t size)
{
    uint32_t id = read_be32(data + 220);

    switch (id) {
        /* TODO
    case EXP_ID_CODE_NUNCHUK:
        if(!nunchuk_handshake(wm,&wm->exp.nunchuk,data,len)) return;
        break;
    case EXP_ID_CODE_GUITAR:
        if(!guitar_hero_3_handshake(wm,&wm->exp.gh3,data,len)) return;
        break;
    case EXP_ID_CODE_WIIBOARD:
        if(!wii_board_handshake(wm,&wm->exp.wb,data,len)) return;
        break;
    case EXP_ID_CODE_CLASSIC_CONTROLLER:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING2:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_NYKOWING3:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC2:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC3:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC4:
    case EXP_ID_CODE_CLASSIC_CONTROLLER_GENERIC5:
    case EXP_ID_CODE_CLASSIC_WIIU_PRO:
    default:
        if(!classic_ctrl_handshake(wm,&wm->exp.classic,data,len)) return;
        break;
*/
    }

    device->exp_state = EXP_STATE_READY;
    device_set_ir_mode(device);
}

/* Returns true if a step was performed */
static bool device_handshake_expansion_step(WpadDevice *device)
{
    uint8_t val;

    switch (device->exp_state) {
    case EXP_STATE_DISCONNECTED:
        device->exp_state = EXP_STATE_ENABLE_1;
        val = 0x55;
        device_write_data(device, WM_EXP_MEM_ENABLE1, &val, 1);
        break;
    case EXP_STATE_ENABLE_1:
        device->exp_state = EXP_STATE_ENABLE_2;
        val = 0x0;
        device_write_data(device, WM_EXP_MEM_ENABLE2, &val, 1);
        break;
    case EXP_STATE_ENABLE_2:
        device->exp_state = EXP_STATE_READ_CALIBRATION;
        device_read_data(device, WM_EXP_MEM_CALIBR, EXP_HANDSHAKE_LEN);
        break;
    default:
        return false;
    }
    return true;
}

static void device_handshake_expansion_start(WpadDevice *device)
{
    WIIUSE_DEBUG("Expansion initialization");
    device_handshake_expansion_step(device);
}

static void device_disable_expansion(WpadDevice *device)
{
    if (device->exp_state == EXP_STATE_DISCONNECTED) return;

    switch (device->exp.type) {
        /* TODO take from wiiuse_disable_expansion */
    }

    device->exp_state = EXP_STATE_DISCONNECTED;
    device->exp.type = EXP_NONE;
    device_set_ir_mode(device);
}

static void process_handshake(WpadDevice *device, uint8_t param,
                              BteBufferReader *reader)
{
    WIIUSE_DEBUG("param %02x, length: %d", param, reader->packet->size - reader->pos_in_packet);
}

static void pressed_buttons(WpadDevice *device, const uint8_t *msg)
{
    uint16_t now = htobe16(*(uint16_t*)msg) & WIIMOTE_BUTTON_ALL;

    /* pressed now & were pressed, then held */
    device->btn_held = (now & device->btn_pressed);

    /* were pressed or were held & not pressed now, then released */
    device->btn_released = ((device->btn_pressed | device->btn_held) & ~now);

    /* buttons pressed now */
    device->btn_pressed = now;

    WIIUSE_DEBUG("Buttons %04x", now);
}

static void device_handshake_completed(WpadDevice *device)
{
    device->state = STATE_HANDSHAKE_COMPLETE;

    int8_t chan = device->unid;
    device_set_leds(device, WIIMOTE_LED_1 << (chan % 4));
}

static bool device_handshake_read_calibration(WpadDevice *device)
{
    device->state = STATE_HANDSHAKE_READ_CALIBRATION;
    return device_read_data(device, WM_MEM_OFFSET_CALIBRATION, 7);
}

static void device_handshake_calibrated(WpadDevice *device,
                                        const uint8_t *data, uint16_t len)
{
    struct accel_t *accel = &device->accel_calibration;

	accel->cal_zero.x = (data[0] << 2) | ((data[3] >> 4) & 3);
	accel->cal_zero.y = (data[1] << 2) | ((data[3] >> 2) & 3);
	accel->cal_zero.z = (data[2] << 2) | (data[3] & 3);

	accel->cal_g.x = ((data[4] << 2) | ((data[7] >> 4) & 3)) - accel->cal_zero.x;
	accel->cal_g.y = ((data[5] << 2) | ((data[7] >> 2) & 3)) - accel->cal_zero.y;
	accel->cal_g.z = ((data[6] << 2) | (data[7] & 3)) - accel->cal_zero.z;

    device_handshake_completed(device);
}

static void event_status(WpadDevice *device, const uint8_t *msg, uint16_t len)
{
    WIIUSE_DEBUG("Status event, length %d", len);
    if (len < 6) return;

    pressed_buttons(device, msg);

    bool critical = false;
    bool attachment = false;
    bool speaker = false;
    bool ir = false;
    if (msg[2] & WM_CTRL_STATUS_BYTE1_BATTERY_CRITICAL) critical = true;
    if (msg[2] & WM_CTRL_STATUS_BYTE1_ATTACHMENT) attachment = true;
    if (msg[2] & WM_CTRL_STATUS_BYTE1_SPEAKER_ENABLED) speaker = true;
    if (msg[2] & WM_CTRL_STATUS_BYTE1_IR_ENABLED) ir = true;

    device->battery_level = msg[5];
    device->battery_critical = critical;

    if (device->state < STATE_HANDSHAKE_COMPLETE) {
        if (device->state == STATE_HANDSHAKE_LEDS_OFF) {
            if (msg[2] & WM_CTRL_STATUS_BYTE1_ATTACHMENT) {
                device->state = STATE_HANDSHAKE_READ_EXPANSION;
                device_read_data(device, WM_EXP_ID, 6);
            } else {
                device_handshake_read_calibration(device);
            }
            return;
        }
    }

    if (!ir && device->ir_enabled) {
        device->ir_enabled = false;
        device_set_ir(device, true);
        return;
    }
    device->ir_enabled = ir;

    if (!speaker && device->speaker_enabled) {
        device->speaker_enabled = false;
        device_set_speaker(device, true);
        return;
    }
    device->speaker_enabled = speaker;

    if (attachment) {
        if (device->exp_state == EXP_STATE_DISCONNECTED) {
            device_handshake_expansion_start(device);
            return;
        }
    } else {
        if (device->exp_state != EXP_STATE_DISCONNECTED) {
            device_disable_expansion(device);
            return;
        }
    }

    device_set_report_type(device);
}

static void event_data_read(WpadDevice *device,
                            const uint8_t *msg, uint16_t len)
{
    if (len < 6) goto error;

    pressed_buttons(device, msg);

    uint8_t err = msg[2] & 0x0f;
    if (err) goto error;

    uint16_t size = (msg[2] >> 4) + 1;
    const uint8_t *data = msg + 5;
    if (device->last_read_data) {
        memcpy(device->last_read_data + device->last_read_cursor, data, size);
        device->last_read_cursor += size;
        if (device->last_read_cursor < device->last_read_size) {
            /* We expect more read events. We'll handle the read when all the
             * data has arrived. */
            return;
        }
        data = device->last_read_data;
        size = device->last_read_size;
    }

    uint32_t offset = device->last_read_offset;
    if (offset == WM_EXP_MEM_CALIBR) {
        device_handshake_expansion_calibrate(device, data, size);
        if (device->state == STATE_HANDSHAKE_READ_EXPANSION) {
            device_handshake_read_calibration(device);
        }
    } else if (offset == WM_MEM_OFFSET_CALIBRATION) {
        device_handshake_calibrated(device, data, size);
    }

error:
    if (device->last_read_data) {
        free(device->last_read_data);
        device->last_read_data = NULL;
    }
    /* This marks the read as completed */
    device->last_read_size = 0;
}

static void parse_event(WpadDevice *device, const uint8_t *report, uint16_t len)
{
    uint8_t event = report[0];
    const uint8_t *msg = report + 1;

    switch (event) {
    case WM_RPT_CTRL_STATUS:
        event_status(device, msg, len - 1);
        break;
    case WM_RPT_READ:
        event_data_read(device, msg, len - 1);
        break;
    default:
        WIIUSE_DEBUG("Event: %02x, length %d", event, len);
    }
}

static void process_data(WpadDevice *device, uint8_t param,
                         BteBufferReader *reader)
{
    WIIUSE_DEBUG("param %02x, length: %d", param, reader->packet->size - reader->pos_in_packet);
    if (param != BTE_HID_REP_TYPE_INPUT) {
        /* Ignore */
        return;
    }

    /* Here it's as being in __wiiuse_receive */
    uint8_t buffer[MAX_PAYLOAD] = { 0, };
    uint16_t len = bte_buffer_reader_read(reader, buffer, sizeof(buffer));
    if (len < 3) return;

    parse_event(device, buffer, len);
}

static void message_received_cb(BteL2cap *l2cap, BteBufferReader *reader,
                                void *userdata)
{
    WpadDevice *device = userdata;
    uint8_t *hdr_ptr = bte_buffer_reader_read_n(reader, 1);
    if (!hdr_ptr) return;

    uint8_t hdr = *hdr_ptr;
    uint8_t type = hdr & BTE_HID_HDR_TRANS_MASK;
    uint8_t param = hdr & BTE_HID_HDR_PARAM_MASK;
    switch (type) {
    case BTE_HID_TRANS_HANDSHAKE:
        process_handshake(device, param, reader);
        break;
    case BTE_HID_TRANS_DATA:
        process_data(device, param, reader);
        break;
    default:
        WIIUSE_DEBUG("got transaction %02x", type);
    }
}

static void on_channels_connected(WpadDevice *device)
{
    /* The BT-HID header tells us which packet type we are reading,
     * therefore we can assing the same callback to both channels */
    bte_l2cap_on_message_received(device->hid_ctrl, message_received_cb);
    bte_l2cap_on_message_received(device->hid_intr, message_received_cb);

    device->state = STATE_HANDSHAKE_LEDS_OFF;
    device_set_leds(device, WIIMOTE_LED_NONE);
    device_request_status(device);
}

static void hid_intr_state_changed_cb(BteL2cap *l2cap, BteL2capState state,
                                      void *userdata)
{
    WpadDevice *device = userdata;

    printf("%s: state %d\n", __func__, state);
    if (state == BTE_L2CAP_OPEN) {
        on_channels_connected(device);
    }
}

static void l2cap_configure_cb(
    BteL2cap *l2cap, const BteL2capConfigureReply *reply, void *userdata)
{
    printf("%s: rejected mask: %08x\n", __func__, reply->rejected_mask);
    if (reply->rejected_mask != 0) {
        bte_l2cap_disconnect(l2cap);
    }
}

static void device_hid_intr_connected(BteL2cap *l2cap)
{
    WpadDevice *device = wpad_device_from_addr(bte_l2cap_get_address(l2cap));
    if (!device) return; /* Should never happen */

    device->hid_intr = bte_l2cap_ref(l2cap);
    bte_l2cap_set_userdata(l2cap, device);
    /* Default configuration parameters are fine */
    bte_l2cap_configure(l2cap, NULL, l2cap_configure_cb, NULL);
    bte_l2cap_on_state_changed(l2cap, hid_intr_state_changed_cb);
}

static void hid_intr_connect_cb(
    BteL2cap *l2cap, const BteL2capConnectionResponse *reply, void *userdata)
{
    printf("%s: result %d, status %d\n", __func__, reply->result, reply->status);

    if (reply->result != BTE_L2CAP_CONN_RESP_RES_OK) {
        return;
    }

    WpadDevice *device = wpad_device_from_addr(bte_l2cap_get_address(l2cap));
    if (!device) return; /* Should never happen */

    device_hid_intr_connected(l2cap);
}

static void hid_ctrl_state_changed_cb(BteL2cap *l2cap, BteL2capState state,
                                      void *userdata)
{
    WpadDevice *device = userdata;
    printf("%s: state %d\n", __func__, state);
    if (state == BTE_L2CAP_OPEN) {
        /* Connect the HID data channel. Since the ACL is already there, the
         * connection parameters are ignored. */
        bte_l2cap_new_outgoing(s_client, &device->address, BTE_L2CAP_PSM_HID_INTR,
                               NULL, 0, hid_intr_connect_cb, NULL);
    }
}

static void device_hid_ctrl_connected(BteL2cap *l2cap)
{
    WpadDevice *device = wpad_device_from_addr(bte_l2cap_get_address(l2cap));
    if (!device) return; /* Should never happen */

    device->hid_ctrl = bte_l2cap_ref(l2cap);
    bte_l2cap_set_userdata(l2cap, device);
    /* Default configuration parameters are fine */
    bte_l2cap_configure(l2cap, NULL, l2cap_configure_cb, NULL);
}

static void hid_ctrl_connect_cb(
    BteL2cap *l2cap, const BteL2capConnectionResponse *reply, void *userdata)
{
    printf("%s: result %d, status %d\n", __func__, reply->result, reply->status);

    if (reply->result != BTE_L2CAP_CONN_RESP_RES_OK) {
        return;
    }

    device_hid_ctrl_connected(l2cap);
    bte_l2cap_on_state_changed(l2cap, hid_ctrl_state_changed_cb);
}

static void device_init(WpadDevice *device, const BteBdAddr *address)
{
    memset(device, 0, sizeof(*device));
    bd_address_copy(&device->address, address);
    device->unid = device - s_devices;
    device->speaker_volume = 0x40;
}

static WpadDevice *device_allocate(const BteBdAddr *address)
{
    for (int slot = 0; slot < WPAD_MAX_DEVICES; slot++) {
        WpadDevice *device = &s_devices[slot];
        if (device->hid_ctrl == NULL) {
            device_init(device, address);
            return device;
        }
    }

    return NULL;
}

static bool add_new_device(BteHci *hci, const BteHciInquiryResponse *info,
                           const char *name)
{
    if (strncmp(name, s_nintendo_rvl, sizeof(s_nintendo_rvl) - 1) != 0) {
        return false;
    }

    int slot = WPAD_MAX_DEVICES;

    /* Found Wii accessory, is it controller or something else? */
    const char *suffix = name + sizeof(s_nintendo_rvl) - 1;
    if (strncmp(suffix, "CNT-", 4) == 0) {
        /* It's an ordinary controller */
        slot = GetActiveSlot(&info->address);
        if (slot < CONF_PAD_MAX_ACTIVE) {
            WIIUSE_DEBUG("Already active in slot %d", slot);
        } else {
            // Not active, try to make active
            slot = WPAD_MAX_DEVICES;
            for (int i = 0; i < CONF_PAD_MAX_ACTIVE; i++) {
                if (!(__wpads_used & (1 << i))) {
                    WIIUSE_DEBUG("Attempting to connect to Wiimote using slot %d", i);
                    slot = i;
                    break;
                }
            }
        }
    } else {
        /* Assume balance board */
        // TODO
    }

    if (slot < WPAD_MAX_DEVICES) {
        WpadDevice *device = &s_devices[slot];
        device_init(device, &info->address);
        BteHciConnectParams params = {
            s_packet_types,
            info->clock_offset,
            info->page_scan_rep_mode,
            true, /* Allow role switch */
        };
        BteL2CapConnectFlags flags = BTE_L2CAP_CONNECT_FLAG_AUTH;
        BteClient *client = bte_hci_get_client(hci);
        bte_l2cap_new_outgoing(client, &info->address, BTE_L2CAP_PSM_HID_CTRL,
                               &params, flags, hid_ctrl_connect_cb, NULL);
        /* TODO: shouldn't we do this only after the connection is successful? */
        __wpads_used |= (0x01 << slot);
    } else {
        WIIUSE_WARNING("WPAD All Slots Used\n");
    }

    return true;
}

static void read_remote_name_cb(BteHci *hci, const BteHciReadRemoteNameReply *r,
                                void *userdata)
{
    if (r->status == 0) {
        printf("Got name %s for " BD_ADDR_FMT "\n", r->name,
               BD_ADDR_DATA(&r->address));

        if (add_new_device(hci, &s_inquiry_responses->responses[s_inquiry_responses->num_names_queried], r->name)) {
            /* We only connect one Wiimote at a time: quit querying for names */
            return;
        }
    }

    s_inquiry_responses->num_names_queried++;
    query_name_next(hci);
}

static void query_name_next(BteHci *hci)
{
    if (s_inquiry_responses->num_names_queried >=
        s_inquiry_responses->num_responses) {
        /* All names have been queried */
        return;
    }

    BteHciInquiryResponse *r =
        &s_inquiry_responses->responses[s_inquiry_responses->num_names_queried];
    bte_hci_read_remote_name(hci, &r->address, r->page_scan_rep_mode, r->clock_offset,
                             NULL, read_remote_name_cb, NULL);
}

static void inquiry_cb(BteHci *hci, const BteHciInquiryReply *reply, void *)
{
    printf("Inquiry done, status = %d\n", reply->status);
    if (reply->status != 0) return;

    printf("Results: %d\n", reply->num_responses);

    BteBdAddr known_devices[MAX_INQUIRY_RESPONSES];
    int num_known_devices = s_inquiry_responses->num_names_queried;
    for (int i = 0; i < num_known_devices; i++) {
        bd_address_copy(&known_devices[i],
                        &s_inquiry_responses->responses[i].address);
    }

    s_inquiry_responses->num_responses = 0;
    s_inquiry_responses->num_names_queried = 0;
    for (int i = 0; i < reply->num_responses; i++) {
        const BteHciInquiryResponse *r = &reply->responses[i];

        bool skip = false;
        for (int j = 0; j < num_known_devices; j++) {
            if (bd_address_is_equal(&r->address, &known_devices[j])) {
                /* Ignore this device, we've already queried its name and it's
                 * not an interesting device. */
                skip = true;
                break;
            }
        }

        uint8_t *b = r->class_of_device.bytes;
        printf(" - " BD_ADDR_FMT " COD %02x%02x%02x offs %d RSSI %d (skip = %d)\n",
               BD_ADDR_DATA(&r->address),
               b[2], b[1], b[0],
               r->clock_offset, r->rssi, skip);
        if (skip) continue;

        /* New device, we'll query its name */
        if (s_inquiry_responses->num_responses >= MAX_INQUIRY_RESPONSES) break;
        BteHciInquiryResponse *resp =
            &s_inquiry_responses->responses[s_inquiry_responses->num_responses++];
        memcpy(resp, r, sizeof(*r));
    }

    query_name_next(hci);
}

s32 WPAD_StopSearch()
{
    BteHci *hci = bte_hci_get(s_client);
    bte_hci_exit_periodic_inquiry(hci, NULL, NULL);
    return ERR_OK;
}

s32 WPAD_Search()
{
    BteHci *hci = bte_hci_get(s_client);
    s_pair_mode = WPAD_PAIR_MODE_TEMPORARY;
    if (!s_inquiry_responses) {
        s_inquiry_responses = malloc(sizeof(*s_inquiry_responses));
        if (!s_inquiry_responses) return ERR_MEM;
    }
    memset(s_inquiry_responses, 0, sizeof (*s_inquiry_responses));
    bte_hci_periodic_inquiry(hci, 4, 5, BTE_LAP_LIAC, 3, 0,
                             NULL, inquiry_cb, NULL);
    return ERR_OK;
}

static bool link_key_request_cb(BteHci *hci, const BteBdAddr *address,
                                void *userdata)
{
    printf("Link key requested from " BD_ADDR_FMT "\n", BD_ADDR_DATA(address));
    if (!wpad_device_is_pairing(address)) return false;

    /* TODO: check if we have a link key... */
    bte_hci_link_key_req_neg_reply(hci, address, NULL, NULL);
    return true;
}

static bool pin_code_request_cb(BteHci *hci, const BteBdAddr *address,
                                void *userdata)
{
    printf("PIN code requested from " BD_ADDR_FMT "\n", BD_ADDR_DATA(address));
    if (!wpad_device_is_pairing(address)) return false;

    const BteBdAddr *pin = s_pair_mode == WPAD_PAIR_MODE_TEMPORARY ?
        address : &s_local_address;
    bte_hci_pin_code_req_reply(hci, address, (uint8_t*)pin, sizeof(*pin), NULL, NULL);
    return true;
}

static bool link_key_notification_cb(
    BteHci *hci, const BteHciLinkKeyNotificationData *data, void *userdata)
{
    printf("Link key notification from " BD_ADDR_FMT ", type %d\n", BD_ADDR_DATA(&data->address), data->key_type);
    if (!wpad_device_is_pairing(&data->address)) return false;

    /* TODO: Store on controller, if not temporary */
    return true;
}

typedef void (*HciNextFunction)(BteHci *hci);

static void generic_command_cb(BteHci *hci, const BteHciReply *reply, void *userdata)
{
    if (reply->status != 0) {
        WIIUSE_ERROR("Command completed with status %d", reply->status);
        return;
    }

    HciNextFunction f = userdata;
    f(hci);
}

static void init_done(BteHci *hci)
{
    //WPAD_Search();
}

static void init_set_page_timeout(BteHci *hci)
{
    bte_hci_write_page_timeout(hci, 0x2000, generic_command_cb, init_done);
}

static void init_set_cod(BteHci *hci)
{
    BteClassOfDevice cod = {{0x04, 0x02,0x40}};
    bte_hci_write_class_of_device(hci, &cod, generic_command_cb,
                                  init_set_page_timeout);
}

static void init_set_inquiry_scan_type(BteHci *hci)
{
    bte_hci_write_inquiry_scan_type(hci, BTE_HCI_INQUIRY_SCAN_TYPE_INTERLACED,
                                    generic_command_cb, init_set_cod);
}

static void init_set_page_scan_type(BteHci *hci)
{
    bte_hci_write_page_scan_type(hci, BTE_HCI_PAGE_SCAN_TYPE_INTERLACED,
                                 generic_command_cb, init_set_inquiry_scan_type);
}

static void init_set_inquiry_mode(BteHci *hci)
{
    bte_hci_write_inquiry_mode(hci, BTE_HCI_INQUIRY_MODE_RSSI,
                               generic_command_cb, init_set_page_scan_type);
}

static void init_set_scan_enable(BteHci *hci)
{
    bte_hci_write_scan_enable(hci, BTE_HCI_SCAN_ENABLE_PAGE,
                              generic_command_cb, init_set_inquiry_mode);
}

static void init_set_local_name(BteHci *hci)
{
    bte_hci_write_local_name(hci, "Wii",
                             generic_command_cb, init_set_scan_enable);
}

static void read_bd_addr_cb(BteHci *hci, const BteHciReadBdAddrReply *reply, void *userdata)
{
    s_local_address = reply->address;
    init_set_local_name(hci);
}

static bool connection_request_cb(BteL2capServer *l2cap_server,
                                  const BteBdAddr *address,
                                  const BteClassOfDevice *cod,
                                  void *userdata)
{
    WIIUSE_DEBUG("from " BD_ADDR_FMT, BD_ADDR_DATA(address));

    /* TODO: check device type, CONF */
    WpadDevice *device = device_allocate(address);
    if (!device) return false; /* No more slots */

    return true;
}

static bool decline_connection(BteL2capServer *l2cap_server,
                               const BteBdAddr *address,
                               const BteClassOfDevice *cod,
                               void *userdata)
{
    WIIUSE_DEBUG("from " BD_ADDR_FMT, BD_ADDR_DATA(address));
    return false;
}

static void incoming_ctrl_connected_cb(
    BteL2capServer *l2cap_server, BteL2cap *l2cap, void *userdata)
{
    WIIUSE_DEBUG("l2cap != NULL %d", l2cap != NULL);
    if (!l2cap) {
        return;
    }

    device_hid_ctrl_connected(l2cap);
}

static void incoming_intr_connected_cb(
    BteL2capServer *l2cap_server, BteL2cap *l2cap, void *userdata)
{
    WIIUSE_DEBUG("l2cap != NULL %d", l2cap != NULL);
    if (!l2cap) {
        /* TODO: deallocate the WpadDevice */
        return;
    }
    device_hid_intr_connected(l2cap);
}

static void initialized_cb(BteHci *hci, bool success, void *)
{
    printf("Initialized, OK = %d\n", success);
    printf("ACL MTU=%d, max packets=%d\n",
           bte_hci_get_acl_mtu(hci),
           bte_hci_get_acl_max_packets(hci));
    BteHciFeatures features = bte_hci_get_supported_features(hci);
    s_packet_types = bte_hci_packet_types_from_features(features);
    bte_hci_on_link_key_request(hci, link_key_request_cb);
    bte_hci_on_pin_code_request(hci, pin_code_request_cb);
    bte_hci_on_link_key_notification(hci, link_key_notification_cb);
    bte_hci_read_bd_addr(hci, read_bd_addr_cb, NULL);

    s_l2cap_server_hid_ctrl = bte_l2cap_server_new(s_client,
                                                   BTE_L2CAP_PSM_HID_CTRL);
    s_l2cap_server_hid_intr = bte_l2cap_server_new(s_client,
                                                   BTE_L2CAP_PSM_HID_INTR);
    bte_l2cap_server_set_needs_auth(s_l2cap_server_hid_ctrl, true);
    bte_l2cap_server_set_role(s_l2cap_server_hid_ctrl, BTE_HCI_ROLE_MASTER);
    bte_l2cap_server_on_connected(s_l2cap_server_hid_ctrl,
                                  incoming_ctrl_connected_cb, NULL);
    bte_l2cap_server_on_connected(s_l2cap_server_hid_intr,
                                  incoming_intr_connected_cb, NULL);
    bte_l2cap_server_on_connection_request(s_l2cap_server_hid_ctrl,
                                           connection_request_cb, NULL);
    /* Since HID clients are required to connect to the control PSM first, the
     * ACL connection is always received on the BteL2capServer handling the
     * control connection. */
    bte_l2cap_server_on_connection_request(s_l2cap_server_hid_intr,
                                           decline_connection, NULL);
}

int main(int argc, char **argv)
{
    quit_requested = false;

    /* Some platforms need to perform some more steps before having the console
     * output setup. */
    terminal_init();

    printf("Initializing...\n");
    s_client = bte_client_new();
    BteHci *hci = bte_hci_get(s_client);
    bte_hci_on_initialized(hci, initialized_cb, NULL);

    while (!quit_requested) {
        bte_wait_events(1000000);
    }

    bte_client_unref(s_client);
    return EXIT_SUCCESS;
}
