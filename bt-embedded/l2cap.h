#ifndef BTE_L2CAP_H
#define BTE_L2CAP_H

#include "buffer.h"
#include "hci.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup l2capG L2CAP API
 * @brief L2CAP protocol layer API
 */

/**
 * @addtogroup l2capG
 * @{
 */

/**
 * @brief L2CAP channel IDs
 *
 * @sa BteL2capChannelIdG
 */
typedef uint16_t BteL2capChannelId;

/** @defgroup BteL2capChannelIdG Predefined channel IDs
 * @{ */
/** Invalid channel */
#define BTE_L2CAP_CHANNEL_ID_NULL       (BteL2capChannelId)0x0000
/** Signalling channel */
#define BTE_L2CAP_CHANNEL_ID_SIGNALLING (BteL2capChannelId)0x0001
/** Connectionless channel */
#define BTE_L2CAP_CHANNEL_ID_RECEPTION  (BteL2capChannelId)0x0002
/** @} */

/**
 * @brief L2CAP Protocol/Service Multiplexer (PSM)
 *
 * @sa BteL2capPsmG
 */
typedef uint16_t BteL2capPsm;

/** @defgroup BteL2capChannelIdG Predefined channel IDs
 * @{ */
#define BTE_L2CAP_PSM_SDP      (BteL2capPsm)0x0001 /**< SDP */
#define BTE_L2CAP_PSM_RFCOMM   (BteL2capPsm)0x0003 /**< RFCOMM */
#define BTE_L2CAP_PSM_TEL_CORD (BteL2capPsm)0x0005 /**< TCS-BIN */
#define BTE_L2CAP_PSM_TCS      (BteL2capPsm)0x0007 /**< TCS-BIN cordless */
#define BTE_L2CAP_PSM_BNEP     (BteL2capPsm)0x000f /**< BNEP */
#define BTE_L2CAP_PSM_HID_CTRL (BteL2capPsm)0x0011 /**< HID control */
#define BTE_L2CAP_PSM_HID_INTR (BteL2capPsm)0x0013 /**< HID interrupt */
#define BTE_L2CAP_PSM_UPNP     (BteL2capPsm)0x0015 /**< UPnP */
#define BTE_L2CAP_PSM_AVCTP    (BteL2capPsm)0x0017 /**< AVCTP */
#define BTE_L2CAP_PSM_AVDTP    (BteL2capPsm)0x0019 /**< AVDTP */
/** @} */

/**
 * @brief Connection response
 *
 * @sa BteL2capConnectCb
 */
typedef struct {
    /** Remote channel ID */
    BteL2capChannelId remote_channel_id;
    /** Local channel ID */
    BteL2capChannelId local_channel_id;
    /** Result of the operation. See @ref BteL2capConnRespResG */
    uint16_t result;
    /** Status of the operation. See @ref BteL2capConnRespStatusG */
    uint16_t status;
} BteL2capConnectionResponse;

/** @defgroup BteL2capConnRespResG Result codes
 * @{ */
#define BTE_L2CAP_CONN_RESP_RES_OK           (uint16_t)0 /**< OK */
/** Connection pending */
#define BTE_L2CAP_CONN_RESP_RES_PENDING      (uint16_t)1
/** Connection refused – PSM not supported */
#define BTE_L2CAP_CONN_RESP_RES_ERR_PSM      (uint16_t)2
/** Connection refused – security block */
#define BTE_L2CAP_CONN_RESP_RES_ERR_SECBLOCK (uint16_t)3
/** Connection refused – no resources available */
#define BTE_L2CAP_CONN_RESP_RES_ERR_RESOURCE (uint16_t)4
/** Connection refused – invalid Source CID */
#define BTE_L2CAP_CONN_RESP_RES_ERR_INV_SCID (uint16_t)6
/** Connection refused – Source CID already allocated */
#define BTE_L2CAP_CONN_RESP_RES_ERR_DUP_SCID (uint16_t)7
/** The peer did not accept our configuration
 * This is a non-standard error code, only emitted by
 * bte_l2cap_new_configured() */
#define BTE_L2CAP_CONN_RESP_RES_CONFIG       (uint16_t)0xF001
/** The ACL connection was dropped
 * This is a non-standard error code, only emitted by
 * bte_l2cap_new_configured() */
#define BTE_L2CAP_CONN_RESP_RES_DISCONNECTED (uint16_t)0xF002

/** @} */

/** @defgroup BteL2capConnRespStatusG Status codes
 * @{ */
/** No further information available */
#define BTE_L2CAP_CONN_RESP_STATUS_NO_INFO        (uint16_t)0
/** Authentication pending */
#define BTE_L2CAP_CONN_RESP_STATUS_AUTHENTICATION (uint16_t)1
/** Authorization pending */
#define BTE_L2CAP_CONN_RESP_STATUS_AUTHORIZATION  (uint16_t)2
/** @} */

/**
 * @brief Callback for the connection request
 *
 * Invoked when the connection request is replied to.
 *
 * @param l2cap The L2CAP handle, or \c NULL if the connection could not be
 *        established. Make sure to call bte_l2cap_ref() on the handle if
 *        you intend to use it, otherwise the object will get destroyed
 *        after this callback returns
 * @param reply The operation reply
 * @param userdata The client data set when the request was issued
 *
 * @sa bte_l2cap_new_outgoing(), bte_l2cap_ref()
 */
typedef void (*BteL2capConnectCb)(
    BteL2cap *l2cap, const BteL2capConnectionResponse *reply, void *userdata);

/**
 * @brief Connection flags
 *
 * Flags for bte_l2cap_new_outgoing() and bte_l2cap_new_configured().
 */
typedef enum {
    /** No flag */
    BTE_L2CAP_CONNECT_FLAG_NONE = 0,
    /** Requires authentication on the connection */
    BTE_L2CAP_CONNECT_FLAG_AUTH = 1 << 0,
} BteL2CapConnectFlags;

/**
 * @brief Connect to a remote device
 *
 * Open a connection to the remote device at \a address for the service \a psm.
 * If there already is an ACL connection to the device, it will be reused;
 * otherwise, a new ACL connection will be established.
 *
 * @param client Handle to the main client object
 * @param address Address of the device to connect to
 * @param psm The Protocol/Service Multiplexer
 * @param params Connection parameters for the ACL connection, only used if
 *        there isn't already a connection to the device. Can be \c NULL,
 *        in which case reasonable defaults will be used
 * @param flags Connection flags
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @note The callback can be invoked more than once, if \a result is \c
 *       BTE_L2CAP_CONN_RESP_RES_PENDING.
 */
void bte_l2cap_new_outgoing(BteClient *client, const BteBdAddr *address,
                            BteL2capPsm psm, const BteHciConnectParams *params,
                            BteL2CapConnectFlags flags,
                            BteL2capConnectCb callback, void *userdata);

/**
 * @brief Add a reference to the L2CAP object
 *
 * Mark the \a l2cap object as in use by incrementing its reference count
 * by one. The object will not be destroyed while the client has at least
 * one reference to it.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The same L2CAP handle
 */
BteL2cap *bte_l2cap_ref(BteL2cap *l2cap);
/**
 * @brief Remove a reference to the L2CAP object
 *
 * Decrement the reference count of \a l2cap by one. When the count reaches
 * zero, the object will be destroyed.
 *
 * @param l2cap The L2CAP handle
 */
void bte_l2cap_unref(BteL2cap *l2cap);

/**
 * @brief Set the user data pointer on the L2CAP object
 *
 * Set the user data pointer on the \a l2cap object to \a userdata. Its value
 * will then be passed as the user data parameter to event callbacks, and can
 * also be retrieved at any moment with bte_l2cap_get_userdata().
 *
 * @param l2cap The L2CAP handle
 * @param userdata The user data pointer
 */
void bte_l2cap_set_userdata(BteL2cap *l2cap, void *userdata);
/**
 * @brief Get the user data pointer
 *
 * Get the user data pointer which was set on the \a l2cap object.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The user data pointer
 */
void *bte_l2cap_get_userdata(BteL2cap *l2cap);

/**
 * @brief Get the connection handle
 *
 * Get the underlying ACL connection's handle.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The ACL connection handle
 */
BteConnHandle bte_l2cap_get_connection_handle(BteL2cap *l2cap);
/**
 * @brief Get the Protocol/Service Multiplexer
 *
 * Get the Protocol/Service Multiplexer (PSM) which this L2CAP channel was
 * created for.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The PSM value
 */
BteL2capPsm bte_l2cap_get_psm(BteL2cap *l2cap);
/**
 * @brief Get the address of the remote device
 *
 * Get the address of the remote device we are connected to.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The BD address
 */
const BteBdAddr *bte_l2cap_get_address(BteL2cap *l2cap);
/**
 * @brief Get the MTU of the local host
 *
 * Get the MTU value for the packet received by the local device.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The local MTU value
 */
uint16_t bte_l2cap_get_mtu(BteL2cap *l2cap);
/**
 * @brief Get the MTU of the remote host
 *
 * Get the MTU value for the packet received by the remote device.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The remote MTU value
 */
uint16_t bte_l2cap_get_remote_mtu(BteL2cap *l2cap);
/**
 * @brief Get the client handle
 *
 * Get the BteClient handle managing this connection.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The client handle
 */
BteClient *bte_l2cap_get_client(BteL2cap *l2cap);
/**
 * @brief Get the HCI handle
 *
 * Get a handle to the HCI controller.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The HCI handle
 */
BteHci *bte_l2cap_get_hci(BteL2cap *l2cap);

/**
 * @brief State of the L2CAP connection
 *
 * State of the L2CAP connection, as described in the "State Machine" section
 * of the Bluetooth documentation.
 */
typedef enum {
    /** Channel not connected */
    BTE_L2CAP_CLOSED = 0,
    /** A connection request has been received, but only a connection response
     * with indication “pending” can be sent */
    BTE_L2CAP_WAIT_CONNECT,
    /** A connection request has been sent, pending a positive connect response
     */
    BTE_L2CAP_WAIT_CONNECT_RSP,
    /** A device has sent or received a connection response, but has neither
     * initiated a configuration request yet, nor received a configuration
     * request with acceptable parameters */
    BTE_L2CAP_WAIT_CONFIG,
    BTE_L2CAP_CONFIG_FIRST = BTE_L2CAP_WAIT_CONFIG,
    /** For the initiator path, a configuration request has not yet been
     * initiated, while for the response path, a request with acceptable
     * options has been received. */
    BTE_L2CAP_WAIT_SEND_CONFIG,
    /** For the initiator path, a request has been sent but a positive response
     * has not yet been received, and for the acceptor path, a request with
     * acceptable options has not yet been received */
    BTE_L2CAP_WAIT_CONFIG_REQ_RSP,
    /** The acceptor path is complete after having responded to acceptable
     * options, but for the initiator path, a positive response on the recent
     * request has not yet been received */
    BTE_L2CAP_WAIT_CONFIG_RSP,
    /** The initiator path is complete after having received a positive
     * response, but for the acceptor path, a request with acceptable options
     * has not yet been received */
    BTE_L2CAP_WAIT_CONFIG_REQ,
    BTE_L2CAP_CONFIG_LAST = BTE_L2CAP_WAIT_CONFIG_REQ,
    /** User data transfer state */
    BTE_L2CAP_OPEN,
    /** A disconnect request has been sent, pending a disconnect response */
    BTE_L2CAP_WAIT_DISCONNECT,
} BteL2capState;

/**
 * @brief Get the L2CAP channel state
 *
 * Get the state of the L2CAP channel.
 *
 * @param l2cap The L2CAP handle
 *
 * @return The channel state
 */
BteL2capState bte_l2cap_get_state(BteL2cap *l2cap);

/**
 * @brief Callback for the state change event
 *
 * Invoked when the channel state changes.
 *
 * @param l2cap The L2CAP handle
 * @param state The channel's current state
 * @param userdata The client data set on the BteL2cap object
 *
 * @sa bte_l2cap_on_state_changed()
 */
typedef void (*BteL2capStateChangedCb)(BteL2cap *l2cap, BteL2capState state,
                                       void *userdata);
/**
 * @brief Watch for state changes
 *
 * Register a callback to be invoked when the state of the L2CAP channel
 * changes.
 *
 * @param l2cap The L2CAP handle
 * @param callback Function to be invoked when the event triggers
 */
void bte_l2cap_on_state_changed(BteL2cap *l2cap,
                                BteL2capStateChangedCb callback);

/**
 * @brief Quality Of Service configuration parameters
 */
typedef struct {
    /** Reserved for future use */
    uint8_t flags;
    /** Level of service required */
    uint8_t service_type;
    /** Average data rate, in bytes per second */
    uint32_t token_rate;
    /** Size of bucket for burst transfer */
    uint32_t token_bucket_size;
    /** How fast data can be transmitted, in bytes per second */
    uint32_t peak_bandwith;
    /** Maximum acceptable delay of an L2CAP packet */
    uint32_t access_latency;
    /** The difference, in microseconds, between the maximum and minimum
     * possible delay of an L2CAP SDU between two L2CAP peers. */
    uint32_t delay_variation;
} BTE_PACKED BteL2capConfigQos;

/**
 * @brief Retransmission and flow control parameters
 */
typedef struct {
    /** Requested mode of the link */
    uint8_t mode;
    /** Size of the transmission window */
    uint8_t tx_window_size;
    /** Number of transmissions of a single I-frame that L2CAP is allowed to
     * try in Retransmission mode and Enhanced Retransmission mode. */
    uint8_t max_transmit;
    /** Value in milliseconds of the retransmission timeout */
    uint16_t retx_timeout;
    /** Value in milliseconds of the interval at which S-frames should be
     * transmitted on the return channel when no frames are received on the
     * forward channel */
    uint16_t monitor_timeout;
    /** The maximum payload size that the L2CAP layer entity sending the option
     * in an L2CAP_CONFIGURATION_REQ packet is capable of accepting */
    uint16_t max_pdu_size;
} BTE_PACKED BteL2capConfigRetxFlow;

/**
 * @brief Extended flow specification
 */
typedef struct {
    /** Unique identifier for the flow specification */
    uint8_t identifier;
    /** The level of service required */
    uint8_t service_type;
    /** The maximum size of the SDUs transmitted by the application */
    uint16_t max_sdu_size;
    /** The time between consecutive SDUs generated by the application */
    uint32_t sdu_inter_time;
    /** The maximum delay between consecutive transmission opportunities on the
     * air-interface for the connection */
    uint32_t access_latency;
    /** Maximum period after which all segments of the SDU are flushed from
     * L2CAP and the Controller */
    uint32_t flush_timeout;
} BTE_PACKED BteL2capConfigExtFlow;

/**
 * @brief L2CAP configuration parameters
 */
typedef struct {
    /** Bit mask of the used fields. See @ref BteL2capConfigFlagsG */
    uint32_t field_mask;
    /** Maximum Transfer Unit (minimum 48 bytes) */
    uint16_t mtu;
    /** Flush timeout, in milliseconds */
    uint16_t flush_timeout;
    /** Frame check sequence */
    uint8_t frame_check_sequence;
    /** Maximum extended window size */
    uint16_t max_window_size;
    /** Quality Of Service options
     * QOS options, or \c NULL. Do not free this data until the configuration
     * process is complete. */
    const BteL2capConfigQos *qos;
    /** Retransmission flow options
     * Can be \c NULL. Do not free this data until the configuration */
    const BteL2capConfigRetxFlow *retx_flow;
    /** Extended flow options
     * Can be \c NULL. Do not free this data until the configuration */
    const BteL2capConfigExtFlow *ext_flow;
} BteL2capConfigureParams;

/** @defgroup BteL2capConfigFlagsG L2CAP configuration fields
 *
 * Bit mask for the configuration fields in the @ref BteL2capConfigureParams
 * structure.
 * @{ */
#define BTE_L2CAP_CONFIG_MTU             (1 << 0) /**< MTU */
#define BTE_L2CAP_CONFIG_FLUSH_TIMEOUT   (1 << 1) /**< Flush timeout */
#define BTE_L2CAP_CONFIG_QOS             (1 << 2) /**< Quality of service */
#define BTE_L2CAP_CONFIG_RETX_FLOW       (1 << 3) /**< Retransmission flow */
#define BTE_L2CAP_CONFIG_FRAME_CHECK_SEQ (1 << 4) /**< Frame check sequence */
#define BTE_L2CAP_CONFIG_EXT_FLOW        (1 << 5) /**< Extended flow */
#define BTE_L2CAP_CONFIG_MAX_WINDOW_SIZE (1 << 6) /**< Maximum window size */
/** @} */

/**
 * @brief L2CAP configuration reply
 */
typedef struct {
    /** Bitmask of the rejected parameters. See @ref BteL2capConfigFlagsG */
    uint32_t rejected_mask;
    /** Bitmask of the unknown parameters. See @ref BteL2capConfigFlagsG */
    uint32_t unknown_mask;
    /** The configuration response */
    BteL2capConfigureParams params;
} BteL2capConfigureReply;

/**
 * @brief Callback for the configuration request
 *
 * Invoked when the configuration request is replied to.
 *
 * @param l2cap The L2CAP handle
 * @param reply The operation reply
 * @param userdata The client data set when the request was issued
 *
 * @sa bte_l2cap_configure()
 */
typedef void (*BteL2capConfigureCb)(
    BteL2cap *l2cap, const BteL2capConfigureReply *reply, void *userdata);

/**
 * @brief Request the L2CAP configuration
 *
 * Initiates the configuration on an L2CAP connection. Note that calling this
 * function is not required if the connection was established with
 * bte_l2cap_new_configured().
 *
 * @param l2cap The L2CAP handle
 * @param params Configuration parameters. Can be \c NULL, in which case an
 *        empty configuration request will be sent, implying that all the
 *        default L2CAP parameters are fine for the local device
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 */
void bte_l2cap_configure(
    BteL2cap *l2cap, const BteL2capConfigureParams *params,
    BteL2capConfigureCb callback, void *userdata);

/**
 * @brief Handler for incoming configuration request events
 *
 * Invoked when a configuration request is received from the remote device. If
 * the request needs to be rejected or corrected,
 * bte_l2cap_set_configure_reply() can be used to send a reply to the remote
 * device.
 *
 * @param l2cap The L2CAP handle
 * @param params Configuration parameters sent by the remote device
 * @param userdata The client data set on the BteL2cap object
 *
 * @sa bte_l2cap_on_configure(), bte_l2cap_set_configure_reply()
 */
typedef void (*BteL2capOnConfigureCb)(
    BteL2cap *l2cap, const BteL2capConfigureParams *params, void *userdata);

/**
 * @brief Watch for configuration requests
 *
 * Register a callback to be invoked when a configuration request is received
 * from the remote device.
 *
 * @param l2cap The L2CAP handle
 * @param callback Function to be invoked when the event triggers
 *
 * @sa bte_l2cap_configure()
 */
void bte_l2cap_on_configure(BteL2cap *l2cap, BteL2capOnConfigureCb callback);
/*!
 * @brief Reply to a configuration request
 *
 * Send a reply to an incoming configuraiton request. This function can only be
 * called from within a @ref BteL2capOnConfigureCb callback.
 *
 * @param l2cap The L2CAP handle
 * @param reply The configuration reply
 */
void bte_l2cap_set_configure_reply(BteL2cap *l2cap,
                                   const BteL2capConfigureReply *reply);

/*!
 * @brief Create an outgoing message
 *
 * Allocate a buffer for an outgoing message of the given size. Once the data
 * has been written, it can be sent with bte_l2cap_send_message():
 *
 * @code
 * BteBufferWriter writer;
 * bool ok = bte_l2cap_create_message(l2cap, &writer, size);
 * if (!ok) { ...handle error... }
 *
 * bte_buffer_writer_write(&writer, "Hello!", 6);
 * int rc = bte_l2cap_send_message(l2cap, bte_buffer_writer_end(&writer));
 * if (rc < 0) { ...handle error... }
 * @endcode
 *
 * @param l2cap The L2CAP handle
 * @param writer A client-allocated @ref BteBufferWriter structure (can be
 *        allocated on the stack)
 * @param size The size in bytes of the message that needs to be sent
 *
 * @return \c true if the function succeeded, \c false otherwise. This can
 * happen if the requested size is bigger than the remote MTU, if we are out of
 * memory, or the \a l2cap object is invalid or not in connected state
 *
 * @sa bte_l2cap_send_message()
 */
bool bte_l2cap_create_message(BteL2cap *l2cap, BteBufferWriter *writer,
                              uint16_t size);
/*!
 * @brief Send a message to the peer
 *
 * Send the payload stored in \a buffer to the peer device.
 *
 * @param l2cap The L2CAP handle
 * @param buffer The data to be sent. The library takes ownership of this
 *        buffer, so if you want to keep it around, add an extra reference to
 *        it before calling this function
 *
 * @return A negative error code on error, 0 if the packet has been sent, or a
 *         positive number telling how many packets are in the outgoing queue.
 */
int bte_l2cap_send_message(BteL2cap *l2cap, BteBuffer *buffer);

/**
 * @brief Handler for incoming data messages
 *
 * Invoked when the remote peer sends some data over the L2CAP channel.
 *
 * @param l2cap The L2CAP handle
 * @param reader A buffer reader through which the data can be retrieved
 * @param userdata The client data set on the BteL2cap object
 *
 * @sa bte_l2cap_on_message_received()
 */
typedef void (*BteL2capMessageReceivedCb)(
    BteL2cap *l2cap, BteBufferReader *reader, void *userdata);
/**
 * @brief Watch for incoming data messages
 *
 * Register a callback to be invoked when a message is received over the L2CAP
 * data channel.
 *
 * @param l2cap The L2CAP handle
 * @param callback Function to be invoked when the event triggers
 *
 * @sa bte_l2cap_send_message()
 */
void bte_l2cap_on_message_received(BteL2cap *l2cap,
                                   BteL2capMessageReceivedCb callback);

/**
 * @brief Callback for the echo request
 *
 * Invoked when the echo request is replied to.
 *
 * @param l2cap The L2CAP handle
 * @param reader A buffer reader for the data send by the remote peer
 * @param userdata The client data set when the request was issued
 *
 * @sa bte_l2cap_echo()
 */
typedef void (*BteL2capEchoCb)(
    BteL2cap *l2cap, BteBufferReader *reader, void *userdata);
/**
 * @brief Send an L2CAP echo request
 *
 * Send an L2CAP echo request with the given data.
 *
 * @param l2cap The L2CAP handle
 * @param data The data to be sent
 * @param size The data size, in bytes
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @return \c true if the request was successfully sent, \c false otherwise.
 */
bool bte_l2cap_echo(BteL2cap *l2cap, const void *data, uint16_t size,
                    BteL2capEchoCb callback, void *userdata);
/**
 * @brief Handler for incoming echo request events
 *
 * Invoked when an echo request is received from the remote device.
 * This callback is first called with the \a writer set to NULL. If the
 * function returns a value bigger than 0, this is interpreted as a sign that
 * the client wants to send that amount of data back to the peer, and then the
 * callback is invoked again with a valid \a writer (on the second invocation,
 * return value should be 0).
 *
 * @param l2cap The L2CAP handle
 * @param reader A buffer reader for the data sent by the remote device
 * @param reader \c NULL the first time this callback is invoked; if the
 *        callback returns a value bigger than zero, then it's invoked again
 *        with a valid @ref BteBufferWriter object, which can be used to
 *        compose the reply to be sent to the remote device
 * @param userdata The client data set on the BteL2cap object
 *
 * @return \c 0 if an empty reply is to be sent, or a value greater than
 *         zero indicating the desired size of the reply; in that case, the
 *         callback will be invoked again and asked to prepare a reply.
 *
 * @sa bte_l2cap_on_echo()
 */
typedef uint16_t (*BteL2capOnEchoCb)(
    BteL2cap *l2cap, BteBufferReader *reader, BteBufferWriter *writer,
    void *userdata);
/**
 * @brief Watch for incoming echo messages
 *
 * Register a callback to be invoked when an echo is received over the L2CAP
 * channel.
 *
 * @param l2cap The L2CAP handle
 * @param callback Function to be invoked when the event triggers
 *
 * @sa bte_l2cap_echo()
 */
void bte_l2cap_on_echo(BteL2cap *l2cap, BteL2capOnEchoCb callback);

/**
 * @brief Informantion type
 *
 * @sa BteL2capInfoTypeG
 */
typedef uint16_t BteL2capInfoType;
/** @defgroup BteL2capInfoTypeG L2CAP information types
 *
 * Information types for the @ref BteL2capInfo structure.
 * @{ */
/** Connectionless MTU */
#define BTE_L2CAP_INFO_TYPE_MTU            (BteL2capInfoType)0x0001
/** Extended features supported */
#define BTE_L2CAP_INFO_TYPE_EXT_FEATURES   (BteL2capInfoType)0x0002
/** Fixed channels supported over BR/EDR */
#define BTE_L2CAP_INFO_TYPE_FIXED_CHANNELS (BteL2capInfoType)0x0003
/** @} */

/** @defgroup BteL2capInfoResultG L2CAP information request result
 *
 * Result codes for the bte_l2cap_query_info() request.
 * @{ */
#define BTE_L2CAP_INFO_RESP_RES_OK          (uint16_t)0 /**< Successful */
#define BTE_L2CAP_INFO_RESP_RES_UNSUPPORTED (uint16_t)1 /**< Not supported */
/** @} */

/**
 * @brief L2CAP information data response
 *
 * Response to the bte_l2cap_query_info() request.
 */
typedef struct {
    /* Determines which field is being returned */
    BteL2capInfoType type;
    /** Result of the operation. See @ref BteL2capInfoResultG */
    uint16_t result;
    /** Union with the response data; check the \a type field to determine
     * which field is being used */
    union {
        /** Connectionless MTU, in bytes */
        uint16_t connectionless_mtu;
        /** Bit mask with the supported extended features */
        uint32_t ext_feature_mask;
        /** Bit mask with the supported fixed channels */
        uint64_t fixed_channels_mask;
    } u;
} BteL2capInfo;

/**
 * @brief Callback for the info request
 *
 * Invoked when the info request is replied to.
 *
 * @param l2cap The L2CAP handle
 * @param info Information request reply
 * @param userdata The client data set when the request was issued
 *
 * @sa bte_l2cap_query_info()
 */
typedef void (*BteL2capInfoCb)(
    BteL2cap *l2cap, const BteL2capInfo *info, void *userdata);
/**
 * @brief Send an L2CAP info request
 *
 * Send an L2CAP info request for the given type.
 *
 * @param l2cap The L2CAP handle
 * @param type The type of information to be retrieved
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @return \c true if the request was successfully sent, \c false otherwise.
 */
bool bte_l2cap_query_info(BteL2cap *l2cap, BteL2capInfoType type,
                          BteL2capInfoCb callback, void *userdata);

/**
 * @brief Callback for the disconnection event
 *
 * This callback is invoked when the L2CAP channel gets disconnected. This
 * might happen as a result of a call to bte_l2cap_disconnect(), or of a L2CAP
 * disconnection request initiated by the remote device, or by a disconnection
 * of the underlying ACL connection (to watch for this latter scenario,
 * register the callback with bte_l2cap_on_acl_disconnected()).
 *
 * @param l2cap The L2CAP handle
 * @param reason Disconnection reason, expressed using HCI error codes (see
 *        @ref BteErrorG). If a disconnection is requested via the L2CAP
 *        protocol, reason is \c BTE_HCI_OTHER_END_CLOSED_CONN_USER if the peer
 *        requested the termination or \c BTE_HCI_CONN_TERMINATED_BY_LOCAL_HOST
 *        if it was initiated by the local host
 * @param userdata The client data set on the BteL2cap object
 */
typedef void (*BteL2capDisconnectCb)(
    BteL2cap *l2cap, uint8_t reason, void *userdata);

/**
 * @brief Send an L2CAP disconnection request
 *
 * Send an L2CAP disconnection request. Call bte_l2cap_on_disconnected() if you
 * want to be notified of the effective termination of the connection.
 *
 * @param l2cap The L2CAP handle
 *
 * @sa bte_l2cap_on_disconnected(), bte_l2cap_on_acl_disconnected()
 */
void bte_l2cap_disconnect(BteL2cap *l2cap);
/**
 * @brief Watch for L2CAP disconnection events
 *
 * Register a callback to be invoked when L2CAP channel gets disconnected.
 *
 * @param l2cap The L2CAP handle
 * @param callback Function to be invoked when the event triggers
 */
void bte_l2cap_on_disconnected(BteL2cap *l2cap, BteL2capDisconnectCb callback);
/**
 * @brief Watch for ACL disconnection events
 *
 * Register a callback to be invoked when underlying ACL connection gets
 * disconnected.
 *
 * @param l2cap The L2CAP handle
 * @param callback Function to be invoked when the event triggers
 */
void bte_l2cap_on_acl_disconnected(BteL2cap *l2cap,
                                   BteL2capDisconnectCb callback);

/**
 * @brief Configured connection response
 */
typedef struct {
    /** Operation result. See @ref BteL2capConnRespResG */
    uint16_t result;
} BteL2capNewConfiguredReply;

/**
 * @brief Callback for the create configured connection request
 *
 * Invoked when the operation initiated with bte_l2cap_new_configured() is completed.
 *
 * @param l2cap The L2CAP handle
 * @param reply Request reply
 * @param userdata The client data set when the request was issued
 *
 * @sa bte_l2cap_new_configured()
 */
typedef void (*BteL2capNewConfiguredCb)(
    BteL2cap *l2cap, const BteL2capNewConfiguredReply *reply, void *userdata);

/**
 * @brief Connect to a remote device and configure the channel
 *
 * Open a connection to the remote device at \a address for the service \a
 * psm and configure the channel according to the parameters given in \a
 * conf. This function is equivalent to calling bte_l2cap_new_outgoing()
 * and then performing the configuration.
 *
 * @param client Handle to the main client object
 * @param address Address of the device to connect to
 * @param psm The Protocol/Service Multiplexer
 * @param params Connection parameters for the ACL connection, only used if
 *        there isn't already a connection to the device. Can be \c NULL,
 *        in which case reasonable defaults will be used
 * @param flags Connection flags
 * @param conf Configuration parameters, or \c NULL to send the default
 *        configuration. Make sure this structure stays valid until the
 *        channel has been established.
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @return \c true if the operation was correctly initiated
 *
 * @note The callback can be invoked more than once, if \a result is \c
 *       BTE_L2CAP_CONN_RESP_RES_PENDING.
 * @note The functin does not copy \a conf: it must remain valid until the
 *       callback is invoked.
 */
bool bte_l2cap_new_configured(
    BteClient *client, const BteBdAddr *address, BteL2capPsm psm,
    const BteHciConnectParams *params, BteL2CapConnectFlags flags,
    const BteL2capConfigureParams *conf,
    BteL2capNewConfiguredCb callback, void *userdata);

/* For testing use only: reset the static variables for the channel and message
 * IDs */
void bte_l2cap_reset();

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BTE_L2CAP_H */
