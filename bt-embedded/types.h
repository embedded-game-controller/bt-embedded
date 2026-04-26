#ifndef BTE_TYPES_H
#define BTE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup types Common types
 * @brief Types used throughout the bt-embedded API
 * @{
 */

/* Common **public** types used by the library and its clients */

#define BTE_PACKED __attribute__((packed))

/**
 * @brief Data buffer
 * @ingroup bufferG
 *
 * Object representing a data buffer, meant to be sent as a packet in the HCI,
 * ACL or L2CAP layers.
 *
 * Buffers are structured as linked lists, where each element can be thought as
 * a packet of data. While the structure members can be seen in the header
 * file, it's recommended that they be not used directly: instead, use the @ref
 * BteBufferReader and @ref BteBufferWriter objects to read and write data
 * buffers.
 */
typedef struct bte_buffer_t BteBuffer;
/**
 * @brief Handle to a bt-embedded client
 *
 * An opaque handle to a bt-embedded client.
 *
 * @sa bte_client_new()
 */
typedef struct bte_client_t BteClient;
/**
 * @brief Handle to the HCI interface
 *
 * An opaque handle to the Host Controller Interface (HCI).
 *
 * @sa bte_hci_get()
 */
typedef struct bte_hci_t BteHci;

/**
 * @brief Handle to an L2CAP channel
 *
 * An opaque handle to an L2CAP channel. L2CAP functions are described in
 * @ref l2capG
 *
 * @sa bte_l2cap_new_outgoing(), bte_l2cap_new_configured()
 */
typedef struct bte_l2cap_t BteL2cap;

/**
 * @brief Address of a bluetooth device
 */
typedef struct {
    /** Bytes forming the address */
    uint8_t bytes[6];
} BteBdAddr;

/**
 * @brief Class of device
 *
 * @sa bte_cod_get_service_class(), bte_cod_get_major_dev_class(),
 *     bte_cod_get_minor_dev_class(), bte_cod_compose()
 */
typedef struct {
    uint8_t bytes[3];
} BteClassOfDevice;

/**
 * @brief Extract the major service class bitfield
 *
 * @param cod The class of device
 * @return The bitfield for the major service classes
 *
 * @sa BteCodServiceClassG
 */
static inline uint16_t bte_cod_get_service_class(BteClassOfDevice cod) {
    return (cod.bytes[2] << 3) | (cod.bytes[1] >> 5);
}

/**
 * @brief Extract the major device class
 *
 * @param cod The class of device
 * @return The major device class
 *
 * @sa BteCodMajorDevClassG
 */
static inline uint8_t bte_cod_get_major_dev_class(BteClassOfDevice cod) {
    return cod.bytes[1] & 0x1f;
}

/**
 * @brief Extract the minor device class
 *
 * @param cod The class of device
 * @return The minor device class
 */
static inline uint8_t bte_cod_get_minor_dev_class(BteClassOfDevice cod) {
    return cod.bytes[0] >> 2;
}

/**
 * @brief Construct a BteClassOfDevice from its components
 *
 * @param service_class Bitmask of service classes (@ref BteCodServiceClassG)
 * @param major_dev_class Major device class (@ref BteCodMajorDevClassG)
 * @param minor_dev_class Minor device class
 *
 * @return The resulting class of device
 */
static inline BteClassOfDevice bte_cod_compose(
    uint16_t service_class, uint8_t major_dev_class, uint8_t minor_dev_class) {
    BteClassOfDevice cod = {{
        (uint8_t)(minor_dev_class << 2),
        (uint8_t)(major_dev_class | (service_class << 5)),
        (uint8_t)(service_class >> 3),
    }};
    return cod;
}

/** @defgroup BteCodServiceClassG Major service classes
 * @{ */
/** Limited Discoverable Mode */
#define BTE_COD_SERVICE_CLASS_LDM       (uint16_t)(1 << 0)
#define BTE_COD_SERVICE_CLASS_LE_AUDIO  (uint16_t)(1 << 1) /**< LE audio */
/** Positioning (local identification) */
#define BTE_COD_SERVICE_CLASS_POSITION  (uint16_t)(1 << 3)
/** Networking (LAN, Ad hoc, ...) */
#define BTE_COD_SERVICE_CLASS_NETWORK   (uint16_t)(1 << 4)
/** Rendering (Printing, Speakers, ...) */
#define BTE_COD_SERVICE_CLASS_RENDER    (uint16_t)(1 << 5)
/** Capturing (Scanner, Microphone, ...) */
#define BTE_COD_SERVICE_CLASS_CAPTURE   (uint16_t)(1 << 6)
/** Object Transfer (v-Inbox, v-Folder, ...) */
#define BTE_COD_SERVICE_CLASS_TRANSFER  (uint16_t)(1 << 7)
/** Audio (Speaker, Microphone, Headset service, ...) */
#define BTE_COD_SERVICE_CLASS_AUDIO     (uint16_t)(1 << 8)
/** Telephony (Cordless telephony, Modem, Headset service, ...) */
#define BTE_COD_SERVICE_CLASS_TELEPHONY (uint16_t)(1 << 9)
/** Information (WEB-server, WAP-server, ...) */
#define BTE_COD_SERVICE_CLASS_INFO      (uint16_t)(1 << 10)
/** @} */

/** @defgroup BteCodMajorDevClassG Major device classes
 * @{ */
/** Miscellaneous */
#define BTE_COD_MAJOR_DEV_CLASS_MISC     (uint8_t)0x0
/** Computer (desktop, notebook, PDA, organizer, ...) */
#define BTE_COD_MAJOR_DEV_CLASS_COMPUTER (uint8_t)0x1
/** Phone (cellular, cordless, pay phone, modem, ...) */
#define BTE_COD_MAJOR_DEV_CLASS_PHONE    (uint8_t)0x2
/** LAN/Network Access Point */
#define BTE_COD_MAJOR_DEV_CLASS_NETWORK  (uint8_t)0x3
/** Audio/Video (headset, speaker, stereo, video display, VCR, ...) */
#define BTE_COD_MAJOR_DEV_CLASS_AV       (uint8_t)0x4
/** Peripheral (mouse, joystick, keyboard, ...) */
#define BTE_COD_MAJOR_DEV_CLASS_PERIPH   (uint8_t)0x5
/** Imaging (printer, scanner, camera, display, ...) */
#define BTE_COD_MAJOR_DEV_CLASS_IMAGING  (uint8_t)0x6
/** Wearable */
#define BTE_COD_MAJOR_DEV_CLASS_WEAR     (uint8_t)0x7
/** Toy */
#define BTE_COD_MAJOR_DEV_CLASS_TOY      (uint8_t)0x8
/** Health */
#define BTE_COD_MAJOR_DEV_CLASS_HEALTH   (uint8_t)0x9
/** Uncategorized (device code not specified) */
#define BTE_COD_MAJOR_DEV_CLASS_UNCAT    (uint8_t)0x1f
/** @} */

/**
 * @brief Link key
 */
typedef struct {
    /** Bytes forming the link key */
    uint8_t bytes[16];
} BteLinkKey;

/**
 * @brief Device Local Address Part (LAP)
 *
 * @sa BteLapG
 */
typedef uint32_t BteLap;
/** @defgroup BteLapG BteLap well-defined values
 * @{ */
#define BTE_LAP_GIAC (BteLap)0x009E8B33 /**< GIAC */
#define BTE_LAP_LIAC (BteLap)0x009E8B00 /**< LIAC */
/** @} */

/**
 * @brief Bluetooth packet type
 *
 * @sa BtePacketTypeG
 */
typedef uint16_t BtePacketType;

/** @defgroup BtePacketTypeG Defined packet types
 * @{ */
#define BTE_PACKET_TYPE_DM1 (BtePacketType)0x0008 /**< DM1 */
#define BTE_PACKET_TYPE_DH1 (BtePacketType)0x0010 /**< DH1 */
#define BTE_PACKET_TYPE_DM3 (BtePacketType)0x0400 /**< DM3 */
#define BTE_PACKET_TYPE_DH3 (BtePacketType)0x0800 /**< DH3 */
#define BTE_PACKET_TYPE_DH5 (BtePacketType)0x4000 /**< DH5 */
#define BTE_PACKET_TYPE_DM5 (BtePacketType)0x8000 /**< DM5 */
/** @} */

/**
 * @brief Connection handle
 *
 * A handle to an ACL or SCO connection.
 *
 * @sa bte_hci_create_connection(), bte_hci_on_connection_request()
 */
typedef uint16_t BteConnHandle;

/* The connection handle is 12 bits, so this value is certainly invalid */
#define BTE_CONN_HANDLE_INVALID (BteConnHandle)0xffff

/**
 * @brief Connection type
 *
 * Describes the type of a connection
 *
 * @sa BteLinkTypeG, bte_hci_create_connection(), bte_hci_on_connection_request()
 */
typedef uint8_t BteLinkType;

/** @defgroup BteLinkTypeG Defined connection types
 * @{ */
#define BTE_LINK_TYPE_SCO  (BteLinkType)0 /**< SCO */
#define BTE_LINK_TYPE_ACL  (BteLinkType)1 /**< ACL */
#define BTE_LINK_TYPE_ESCO (BteLinkType)2 /**< eSCO */
/** @} */

/**
 * @brief Encryption mode
 *
 * @sa BteEncryptionModeG, BteHciCreateConnectionReply, BteHciAcceptConnectionReply
 */
typedef uint8_t BteEncryptionMode;

/** @defgroup BteEncryptionModeG Encryption modes
 * @{ */
#define BTE_ENCRYPTION_MODE_NONE (BteEncryptionMode)0 /**< Encryption disabled */
#define BTE_ENCRYPTION_MODE_ALL  (BteEncryptionMode)1 /**< Encryption enabled */
/** @} */

/**
 * @brief Device role
 *
 * @sa BteRoleG, bte_hci_accept_connection()
 */
typedef uint8_t BteRole;

/** @defgroup BteRoleG Device roles
 * @{ */
#define BTE_HCI_ROLE_MASTER (BteRole)0 /**< Master */
#define BTE_HCI_ROLE_SLAVE  (BteRole)1 /**< Slave */
/** @} */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
