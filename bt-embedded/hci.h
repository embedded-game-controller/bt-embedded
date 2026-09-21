#ifndef BTE_HCI_H
#define BTE_HCI_H

#include "hci_proto.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup hci HCI API
 * @brief Host Controller Interface (HCI) API
 */

/**
 * @addtogroup hci
 * @{
 */

/**
 * @brief Link policy settings
 *
 * @sa BteHciLinkPolicySettingsG
 */
typedef uint16_t BteHciLinkPolicySettings;

/**
 * @brief Get a HCI handle
 *
 * Get a handle to be used with `bte_hci_*` functions.
 *
 * @param client Handle to the main client object.
 *
 * @returns Opaque handle, which will be valid as long as the \a client object
 *          stays alive.
 */
BteHci *bte_hci_get(BteClient *client);

/**
 * @brief Get the client handle
 *
 * Get a handle to the main client object.
 *
 * @param hci The HCI handle
 *
 * @return The handle to the client object.
 */
BteClient *bte_hci_get_client(BteHci *hci);

/**
 * @brief Callback for the bte_hci_on_initialized() function.
 *
 * This callback will be invoked when the HCI controller has been initialized.
 * @param hci The HCI handle
 * @param success Whether the controlled was successfully initialized
 * @param userdata Client data.
 *
 * @sa bte_hci_on_initialized()
 */
typedef void (*BteInitializedCb)(BteHci *hci, bool success, void *userdata);
/**
 * @brief Notify when the HCI controller has been initialized.
 *
 * Initialized the HCI controller. Call this function before invoking any other
 * `bte_hci_*` functions.
 * @param hci The HCI handle
 * @param callback Function to be invoked when the controller is ready. Note
 *        that the callback might be called immediately even before
 *        bte_hci_on_initialized() returns, if the controlles has already been
 *        initialized.
 * @param userdata Client data to be passed to the callback.
 */
void bte_hci_on_initialized(BteHci *hci, BteInitializedCb callback,
                            void *userdata);

/**
 * @brief Mask for supported HCI features
 *
 * @sa BteHciFeaturesG
 */
typedef uint64_t BteHciFeatures;
/**
 * @brief Get the features supported by the HCI controller
 *
 * @param hci The HCI handle
 * @return A mask of supported features
 */
BteHciFeatures bte_hci_get_supported_features(BteHci *hci);

/**
 * @brief Get the local host's BD address
 *
 * Return the host's bluetooth device address. If this function is called
 * before the device has been initialized, it will return false and no address
 * will be copied into \a address.
 *
 * @param hci The HCI handle
 * @param address Pointer to where the address will be stored
 * @return \c true if the address was read, \c false otherwise
 */
bool bte_hci_get_bd_address(BteHci *hci, BteBdAddr *address);

/**
 * @brief Get the ACL MTU
 *
 * @param hci The HCI handle
 * @return The MTU for ACL connections
 */
uint16_t bte_hci_get_acl_mtu(BteHci *hci);

/**
 * @brief Get the SCO MTU
 *
 * @param hci The HCI handle
 * @return The MTU for SCO connections
 */
uint8_t bte_hci_get_sco_mtu(BteHci *hci);

/**
 * @brief Get the max ACL packet count
 *
 * @param hci The HCI handle
 * @return The total number of HCI ACL Data packets that can be stored in the
 *         data buffers of the controller.
 */
uint16_t bte_hci_get_acl_max_packets(BteHci *hci);

/**
 * @brief Get the max SCO packet count
 *
 * @param hci The HCI handle
 * @return The total number of HCI SCO packets that can be stored in the data
 *         buffers of the controller.
 */
uint16_t bte_hci_get_sco_max_packets(BteHci *hci);

/**
 * @brief Computes the supported packet types from features
 *
 * @param features The bitmask of features supported by the ACL controller.
 * @return The bitmask of the supported packet types
 *
 * @sa bte_hci_get_supported_features(), BteHciConnectParams
 */
BtePacketType bte_hci_packet_types_from_features(
    BteHciFeatures features);

/**
 * @brief Base structure for HCI asynchronous replies
 *
 * All command replies start with this struct
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
} BteHciReply;

/**
 * @brief Command completion or command status callback
 *
 * This callback is used to asynchronously deliver the result of functions
 * which don't have additional return parameters.
 *
 * @param hci The HCI handle
 * @param reply Contains the result code of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciDoneCb)(BteHci *hci, const BteHciReply *reply,
                             void *userdata);

/* TODO remove */
void bte_hci_nop(BteHci *hci, BteHciDoneCb callback, void *userdata);

/**
 * @defgroup LinkControlG Link control commands
 * @{
 */

/**
 * @brief Describe a discovered device
 */
typedef struct {
    /** Address of the discovered device */
    BteBdAddr address;
    /** Page scan repetition mode */
    uint8_t page_scan_rep_mode;
    /** Page scan period mode */
    uint8_t page_scan_period_mode;
    uint8_t reserved;
    /** Class of device */
    BteClassOfDevice class_of_device;
    /** Clock offset */
    uint16_t clock_offset;
    /** Received signal strength indicator
     * @note Only set if inquiry mode is BTE_HCI_INQUIRY_MODE_RSSI */
    uint8_t rssi;
    /** Pointer to the extended response data
     * @note Only set if inquiry mode is BTE_HCI_INQUIRY_MODE_EXTENDED
     *       (currently not implemented) */
    void *extended_response;
} BTE_PACKED BteHciInquiryResponse;

/**
 * @brief Result of the inquiry command
 *
 * This structure is used to deliver the result of the bte_hci_inquiry() and
 * bte_hci_periodic_inquiry() commands.
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Number of discovered devices */
    uint8_t num_responses;
    /** Array of discovered devices */
    const BteHciInquiryResponse *responses;
} BteHciInquiryReply;

/**
 * @brief Inquiry callback
 *
 * This callback is invoked when an inquiry operation (or an inquiry period)
 * has completed.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the inquiry
 * @param userdata The same client data specified when invoking the command.
 *
 * @sa bte_hci_inquiry(), bte_hci_periodic_inquiry()
 */
typedef void (*BteHciInquiryCb)(
    BteHci *hci, const BteHciInquiryReply *reply, void *userdata);
/**
 * @brief Start an inquiry operation
 *
 * Start a scan for discoverable devices.
 *
 * @param hci The HCI handle
 * @param lap Local Address Part (LAP) to scan for
 * @param len Duration of the inquiry; multiple by 1.28 to get seconds
 * @param max_resp Maximum number of responses
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function to be invoked on completion
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_periodic_inquiry(), bte_hci_inquiry_cancel()
 */
void bte_hci_inquiry(BteHci *hci, BteLap lap, uint8_t len, uint8_t max_resp,
                     BteHciDoneCb status_cb,
                     BteHciInquiryCb callback, void *userdata);
/**
 * @brief Cancel an inquiry operation
 *
 * Stop the scan for discoverable devices.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked on completion (can be \c NULL)
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_inquiry()
 */
void bte_hci_inquiry_cancel(BteHci *hci, BteHciDoneCb callback,
                            void *userdata);
/**
 * @brief Start a periodic inquiry
 *
 * Start a periodic scan for discoverable devices. This is similar to
 * reinvoking bte_hci_inquiry() every time the scan completes, but it's handled
 * at the HCI level.
 *
 * @param hci The HCI handle
 * @param min_period Minimal time between each inquiry (in 1.28 seconds)
 * @param max_period Maxmimum time between each inquiry (in 1.28 seconds)
 * @param lap Local Address Part (LAP) to scan for
 * @param len Duration of the inquiry; multiple by 1.28 to get seconds
 * @param max_resp Maximum number of responses
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function to be invoked on every time an inquiry run
 *        completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_inquiry(), bte_hci_periodic_inquiry_cancel()
 */
void bte_hci_periodic_inquiry(BteHci *hci,
                              uint16_t min_period, uint16_t max_period,
                              BteLap lap, uint8_t len, uint8_t max_resp,
                              BteHciDoneCb status_cb,
                              BteHciInquiryCb callback, void *userdata);
/**
 * @brief Cancel the periodic inquiry
 *
 * Stop the periodic scan for discoverable devices.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked on completion (can be \c NULL)
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_periodic_inquiry()
 */
void bte_hci_exit_periodic_inquiry(BteHci *hci,
                                   BteHciDoneCb callback, void *userdata);

/** Invalid clock offset */
#define BTE_HCI_CLOCK_OFFSET_INVALID (uint16_t)0xffff

/**
 * @brief Parameters for connecting to a device
 *
 * @sa bte_hci_create_connection()
 */
typedef struct {
    /** Packet types supported by the local controller */
    BtePacketType packet_type;
    /** Clock offset; use @ref BTE_HCI_CLOCK_OFFSET_INVALID if unknown */
    uint16_t clock_offset;
    /** Page scan repetition mode */
    uint8_t page_scan_rep_mode;
    /** Whether role switch is allowed */
    bool allow_role_switch;
} BteHciConnectParams;

/**
 * @brief New connection information
 *
 * This structure describes a new connection.
 *
 * @note While the same structure is also used for incoming connections, it's
 *       recommended to use the BteHciAcceptConnectionReply typedef when
 *       handling incoming connections.
 * @sa bte_hci_create_connection(), bte_hci_accept_connection()
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection type */
    BteLinkType link_type;
    /** Handle of the new connection */
    BteConnHandle conn_handle;
    /** BD address of the peer */
    BteBdAddr address;
    /** Encryption mode */
    BteEncryptionMode encryption_mode;
} BteHciCreateConnectionReply;

/**
 * @brief Create connection callback
 *
 * Invoked when the creation of the connection has completed.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 *
 * @sa bte_hci_create_connection()
 */
typedef void (*BteHciCreateConnectionCb)(
    BteHci *hci, const BteHciCreateConnectionReply *reply, void *userdata);
/**
 * @brief Connect to a remote device
 *
 * Create an outgoing connection to the device at \a address.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device to connect to
 * @param params Connection parameters
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function to be invoked when the connection has been created
 *        (or in case of failure)
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_create_connection_cancel(), bte_hci_disconnect()
 */
void bte_hci_create_connection(BteHci *hci,
                               const BteBdAddr *address,
                               const BteHciConnectParams *params,
                               BteHciDoneCb status_cb,
                               BteHciCreateConnectionCb callback,
                               void *userdata);
/**
 * @brief Close a connection
 *
 * Close the connection identified by \a handle.
 *
 * @param hci The HCI handle
 * @param handle Handle of the connection to be terminated
 * @param reason Reason for terminating the connection. This must be one of
 *        @ref BteErrorG allowed by the Bluetooth specification for the HCI
 *        Disconnect command.
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_disconnect()
 */
void bte_hci_disconnect(BteHci *hci, BteConnHandle handle, uint8_t reason,
                        BteHciDoneCb callback, void *userdata);
/**
 * @brief Cancel the creation of a connection
 *
 * Cancel the operation of connecting to the device at \a address.
 *
 * @param hci The HCI handle
 * @param address BD address of the remote device
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_create_connection(), bte_hci_disconnect()
 */
void bte_hci_create_connection_cancel(BteHci *hci, const BteBdAddr *address,
                                      BteHciDoneCb callback, void *userdata);

/**
 * @brief Number of completed packets on a connection
 *
 * @sa bte_hci_on_nr_of_completed_packets()
 */
typedef struct {
    /** Handle of the connection */
    BteConnHandle conn_handle;
    /** Number of the completed packets since the last callback invocation */
    uint16_t completed_packets;
} BteHciNrOfCompletedPacketsData;

/**
 * @brief Callback for the completed packets event
 *
 * Invoked when the HCI controller reports the number of completed packets.
 *
 * @param hci The HCI handle
 * @param data Information on the completed packets
 * @param userdata The client data set on the BteClient object
 *
 * @return \c true if the client has handled the event (this should be the case
 *         if the connection handle belongs to this client)
 * @sa bte_hci_on_nr_of_completed_packets()
 */
typedef bool (*BteHciNrOfCompletedPacketsCb)(
    BteHci *hci, const BteHciNrOfCompletedPacketsData *data, void *userdata);
/**
 * @brief Watch emissions of the Nr Of Completed Packets event
 *
 * Register a callback to be invoked every time that the "Number Of Completed
 * Packets" event is emitted by the HCI controller.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked when the event triggers
 */
void bte_hci_on_nr_of_completed_packets(
    BteHci *hci, BteHciNrOfCompletedPacketsCb callback);

/**
 * @brief Disconnection information
 *
 * @sa bte_hci_disconnect(), bte_hci_on_disconnection_complete()
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Reason for terminating the connection. This is one of @ref BteErrorG */
    uint8_t reason;
    /** Handle of the connection which was terminated */
    BteConnHandle conn_handle;
} BteHciDisconnectionCompleteData;

/**
 * @brief Callback for the disconnection complete event
 *
 * Invoked when the HCI controller reports that a connection was terminated.
 *
 * @param hci The HCI handle
 * @param data Information on the terminated connection
 * @param userdata The client data set on the BteClient object
 *
 * @return \c true if the client has handled the event (this should be the case
 *         if the connection handle belongs to this client)
 * @sa bte_hci_on_disconnection_complete(), bte_hci_disconnect()
 */
typedef bool (*BteHciDisconnectionCompleteCb)(
    BteHci *hci, const BteHciDisconnectionCompleteData *data, void *userdata);
/**
 * @brief Watch disconnection events
 *
 * Register a callback to be invoked every time that a connection gets
 * terminated. Callbacks are invoked on the clients in the order they
 * subscribed; if a client returns \c true from the callback, the event will
 * not be further propagated to other clients.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked when the event triggers
 */
void bte_hci_on_disconnection_complete(
    BteHci *hci, BteHciDisconnectionCompleteCb callback);

/**
 * @brief Reply for bte_hci_accept_connection()
 *
 * This is an alias to the @ref BteHciCreateConnectionReply structure
 */
typedef BteHciCreateConnectionReply BteHciAcceptConnectionReply;
/**
 * @brief Accept connection callback
 *
 * Invoked when the accepted connection has been created.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 *
 * @sa bte_hci_accept_connection()
 */
typedef void (*BteHciAcceptConnectionCb)(
    BteHci *hci, const BteHciAcceptConnectionReply *reply, void *userdata);
/**
 * @brief Accept a connection from a remote device
 *
 * Accept the incoming connection from the device at \a address.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device to connect to
 * @param role The desired role assumed by the local device
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function to be invoked when the connection has been created
 *        (or in case of failure)
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_on_connection_request(), bte_hci_reject_connection()
 */
void bte_hci_accept_connection(BteHci *hci,
                               const BteBdAddr *address, BteRole role,
                               BteHciDoneCb status_cb,
                               BteHciAcceptConnectionCb callback,
                               void *userdata);

/**
 * @brief Reply for bte_hci_reject_connection()
 *
 * This is an alias to the @ref BteHciCreateConnectionReply structure
 */
typedef BteHciCreateConnectionReply BteHciRejectConnectionReply;
/**
 * @brief Reject connection callback
 *
 * Invoked when the connection has been rejected.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 *
 * @sa bte_hci_reject_connection()
 */
typedef void (*BteHciRejectConnectionCb)(
    BteHci *hci, const BteHciRejectConnectionReply *reply, void *userdata);
/**
 * @brief Reject a connection from a remote device
 *
 * Reject the incoming connection from the device at \a address.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device to connect to
 * @param reason Reason for rejecting the connection. This must be one of
 *        @ref BteErrorG allowed by the Bluetooth specification for the HCI
 *        Reject Connection command.
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function to be invoked when the connection has been rejected
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_on_connection_request(), bte_hci_accept_connection()
 */
void bte_hci_reject_connection(BteHci *hci,
                               const BteBdAddr *address, uint8_t reason,
                               BteHciDoneCb status_cb,
                               BteHciRejectConnectionCb callback,
                               void *userdata);

/**
 * @brief Callback for incoming connections.
 *
 * This function is called when an incoming connection is received. If the
 * client does not care about this connection, it should return \c false,
 * to let any other subscribed client to handle it instead. If the client
 * returns \c true, it is expected that it calls
 * bte_hci_accept_connection() or bte_hci_reject_connection(), and no other
 * clients will receive this callback. If all clients return \c false, the
 * connection request will time out.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device to connect to
 * @param cod The class of the remote device
 * @param link_type The connection type
 * @param userdata The client data set on the BteClient object
 */
typedef bool (*BteHciConnectionRequestCb)(BteHci *hci,
                                          const BteBdAddr *address,
                                          const BteClassOfDevice *cod,
                                          BteLinkType link_type,
                                          void *userdata);
/**
 * @brief Watch for incoming connections
 *
 * Register a callback to be invoked every time that a remote device attempts
 * to connect to our device.
 * Callbacks are invoked on the clients in the order they subscribed; if a
 * client returns \c true from the callback, the event will not be further
 * propagated to other clients.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked on an incoming connection
 */
void bte_hci_on_connection_request(
    BteHci *hci, BteHciConnectionRequestCb callback);

/**
 * @brief Callback for link key requests
 *
 * This function is called when the HCI controller needs to provide a link key
 * to a remote device.
 * If the client does not care about this connection, it should return \c
 * false, to let any other subscribed client to handle it instead. If the
 * client returns \c true, it is expected that it calls
 * bte_hci_link_key_req_reply() or bte_hci_link_key_req_neg_reply(), and no
 * other clients will receive this callback.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device we are connecting to
 * @param userdata The client data set on the BteClient object
 *
 * @sa bte_hci_on_link_key_request()
 */
typedef bool (*BteHciLinkKeyRequestCb)(BteHci *hci,
                                       const BteBdAddr *address,
                                       void *userdata);
/**
 * @brief Watch for link key requests
 *
 * Register a callback to be invoked every time that a link key is requested to
 * authenticate to a remote device.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked on an link key request
 */
void bte_hci_on_link_key_request(
    BteHci *hci, BteHciLinkKeyRequestCb callback);

/**
 * @brief Result of a link key reply
 *
 * @sa bte_hci_link_key_req_reply(), bte_hci_link_key_req_neg_reply()
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** BD address of the peer */
    BteBdAddr address;
} BteHciLinkKeyReqReply;

/**
 * @brief Result callback for link key reply
 *
 * Invoked when the link key reply has been sent.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 *
 * @sa bte_hci_link_key_req_reply(), bte_hci_link_key_req_neg_reply()
 */
typedef void (*BteHciLinkKeyReqReplyCb)(BteHci *hci,
                                        const BteHciLinkKeyReqReply *reply,
                                        void *userdata);
/**
 * @brief Reply to a link key request
 *
 * Provide a link key to the HCI controller, to authenticate with the remote
 * device.
 *
 * @param hci The HCI handle
 * @param address The BD address of the remote device
 * @param key Link key
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_link_key_req_neg_reply(), bte_hci_on_link_key_request()
 */
void bte_hci_link_key_req_reply(BteHci *hci, const BteBdAddr *address,
                                const BteLinkKey *key,
                                BteHciLinkKeyReqReplyCb callback,
                                void *userdata);
/**
 * @brief Decline a link key request
 *
 * Negatively replies to a link key request, without providing a link key.
 *
 * @param hci The HCI handle
 * @param address The BD address of the remote device
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_link_key_req_reply(), bte_hci_on_link_key_request()
 */
void bte_hci_link_key_req_neg_reply(BteHci *hci, const BteBdAddr *address,
                                    BteHciLinkKeyReqReplyCb callback,
                                    void *userdata);

/**
 * @brief Link key notification data
 */
typedef struct {
    /** BD address of the peer */
    BteBdAddr address;
    /** Link key */
    BteLinkKey key;
    /** Type of the key. See @ref BteHciLinkKeyTypeG */
    uint8_t key_type;
} BteHciLinkKeyNotificationData;

/** @defgroup BteHciLinkKeyTypeG Types of link keys
 * @{ */
#define BTE_HCI_LINK_KEY_TYPE_COMBINATION (uint8_t)0 /**< Combination Key */
#define BTE_HCI_LINK_KEY_TYPE_LOCAL       (uint8_t)1 /**< Local unit key */
#define BTE_HCI_LINK_KEY_TYPE_REMOTE      (uint8_t)2 /**< Remote unit key */
/** @} */

/**
 * @brief Callback for link key notifications
 *
 * This function is called when a link key to a remote device has been setup.
 * If the client does not care about this device, it should return \c false, to
 * let any other subscribed client to handle it instead. If the
 * client returns \c true, no other clients will receive this callback.
 *
 * @param hci The HCI handle
 * @param userdata The client data set on the BteClient object
 *
 * @sa bte_hci_on_link_key_notification()
 */
typedef bool (*BteHciLinkKeyNotificationCb)(
    BteHci *hci, const BteHciLinkKeyNotificationData *data, void *userdata);
/**
 * @brief Watch for link key notifications
 *
 * Register a callback to be invoked every time that a link key is received.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked on an link key notification
 */
void bte_hci_on_link_key_notification(
    BteHci *hci, BteHciLinkKeyNotificationCb callback);

/**
 * @brief Callback for PIN code requests
 *
 * This function is called when the HCI controller needs to provide a PIN code
 * to a remote device.
 * If the client does not care about this connection, it should return \c
 * false, to let any other subscribed client to handle it instead. If the
 * client returns \c true, it is expected that it calls
 * bte_hci_pin_code_req_reply() or bte_hci_pin_code_req_neg_reply(), and no
 * other clients will receive this callback.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device we are connecting to
 * @param userdata The client data set on the BteClient object
 *
 * @sa bte_hci_on_pin_code_request()
 */
typedef bool (*BteHciPinCodeRequestCb)(BteHci *hci,
                                       const BteBdAddr *address,
                                       void *userdata);
/**
 * @brief Watch for PIN code requests
 *
 * Register a callback to be invoked every time that a PIN code is requested to
 * authenticate to a remote device.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked on an PIN code request
 */
void bte_hci_on_pin_code_request(BteHci *hci, BteHciPinCodeRequestCb callback);

/**
 * @brief Result of a PIN code reply
 *
 * @sa bte_hci_pin_code_req_reply(), bte_hci_pin_code_req_neg_reply()
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** BD address of the peer */
    BteBdAddr address;
} BteHciPinCodeReqReply;

/**
 * @brief Result callback for PIN code reply
 *
 * Invoked when the PIN code reply has been sent.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 *
 * @sa bte_hci_pin_code_req_reply(), bte_hci_pin_code_req_neg_reply()
 */
typedef void (*BteHciPinCodeReqReplyCb)(BteHci *hci,
                                        const BteHciPinCodeReqReply *reply,
                                        void *userdata);
/**
 * @brief Reply to a PIN code request
 *
 * Provide a PIN code to the HCI controller, to authenticate with the remote
 * device.
 *
 * @param hci The HCI handle
 * @param address The BD address of the remote device
 * @param pin The PIN code
 * @param len Length of the PIN code
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_pin_code_req_neg_reply(), bte_hci_on_pin_code_request()
 */
void bte_hci_pin_code_req_reply(BteHci *hci, const BteBdAddr *address,
                                const uint8_t *pin, uint8_t len,
                                BteHciPinCodeReqReplyCb callback,
                                void *userdata);
/**
 * @brief Decline a PIN code request
 *
 * Negatively replies to a PIN code request, without providing a PIN code.
 *
 * @param hci The HCI handle
 * @param address The BD address of the remote device
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_pin_code_req_reply(), bte_hci_on_pin_code_request()
 */
void bte_hci_pin_code_req_neg_reply(BteHci *hci, const BteBdAddr *address,
                                    BteHciPinCodeReqReplyCb callback,
                                    void *userdata);

/**
 * @brief Authentication request data
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
} BteHciAuthRequestedReply;

/**
 * @brief Result callback for bte_hci_auth_requested()
 *
 * Invoked when the authentication has been completed.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciAuthRequestedCb)(
    BteHci *hci, const BteHciAuthRequestedReply *reply, void *userdata);
/**
 * @brief Request authentication on a connection
 *
 * Request the authentication on the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 */
void bte_hci_auth_requested(BteHci *hci,
                            BteConnHandle conn_handle,
                            BteHciDoneCb status_cb,
                            BteHciAuthRequestedCb callback, void *userdata);

/**
 * @brief Read remote name reply
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** BD address of the peer */
    BteBdAddr address;
    /** Name of the remote device. Guaranteed to be 0-terminated */
    char name[248 + 1];
} BteHciReadRemoteNameReply;

/**
 * @brief Result callback for bte_hci_read_remote_name()
 *
 * Invoked when the name of the remove device has been retrieved.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadRemoteNameCb)(
    BteHci *hci, const BteHciReadRemoteNameReply *reply, void *userdata);
/**
 * @brief Read the name of a remote device
 *
 * Read the name of the device at \a address.
 *
 * @param hci The HCI handle
 * @param address The BD address of the remote device
 * @param page_scan_rep_mode The page scan repetition mode
 * @param clock_offset The clock offset, or @ref BTE_HCI_CLOCK_OFFSET_INVALID
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa BteHciInquiryResponse
 */
void bte_hci_read_remote_name(BteHci *hci,
                              const BteBdAddr *address,
                              uint8_t page_scan_rep_mode,
                              uint16_t clock_offset,
                              BteHciDoneCb status_cb,
                              BteHciReadRemoteNameCb callback, void *userdata);

/**
 * @brief Read remote features reply
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** The bitmask of features supported by the remove device */
    BteHciFeatures features;
} BteHciReadRemoteFeaturesReply;

/**
 * @brief Result callback for bte_hci_read_remote_features()
 *
 * Invoked when the feature mask for the remote peer has been retrieved.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadRemoteFeaturesCb)(
    BteHci *hci, const BteHciReadRemoteFeaturesReply *reply, void *userdata);
/**
 * @brief Read features supported by the remote device
 *
 * Request the mask of supported features on the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_local_features()
 */
void bte_hci_read_remote_features(BteHci *hci,
                                  BteConnHandle conn_handle,
                                  BteHciDoneCb status_cb,
                                  BteHciReadRemoteFeaturesCb callback,
                                  void *userdata);

/**
 * @brief Read remote version info reply
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** Link manager protocol version */
    uint8_t lmp_version;
    /** Link manager protocol version */
    uint16_t lmp_subversion;
    /** Identifier of the device manufacturer */
    uint16_t manufacturer_name;
} BteHciReadRemoteVersionInfoReply;

/**
 * @brief Result callback for bte_hci_read_remote_version_info()
 *
 * Invoked when the remote version information has been retrieved.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadRemoteVersionInfoCb)(
    BteHci *hci, const BteHciReadRemoteVersionInfoReply *reply,
    void *userdata);
/**
 * @brief Read version information from the remote device
 *
 * Request the version information for the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_local_version()
 */
void bte_hci_read_remote_version_info(BteHci *hci,
                               BteConnHandle conn_handle,
                               BteHciDoneCb status_cb,
                               BteHciReadRemoteVersionInfoCb callback,
                               void *userdata);

/**
 * @brief Read clock offset reply
 *
 * @sa BteHciReadClockOffsetCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** Clock offset */
    uint16_t clock_offset;
} BteHciReadClockOffsetReply;

/**
 * @brief Result callback for bte_hci_read_clock_offset()
 *
 * Invoked when the remote clock offset has been retrieved.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadClockOffsetCb)(
    BteHci *hci, const BteHciReadClockOffsetReply *reply, void *userdata);
/**
 * @brief Read the clock offset for the remote device
 *
 * Request the clock offset for the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param status_cb Status callback, can be \c NULL
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 */
void bte_hci_read_clock_offset(BteHci *hci,
                               BteConnHandle conn_handle,
                               BteHciDoneCb status_cb,
                               BteHciReadClockOffsetCb callback,
                               void *userdata);

/**
 * @}
 */

/**
 * @defgroup LinkPolicyG Link policy commands
 * @{
 */

/** @defgroup BteHciModeG Device modes
 * @sa bte_hci_on_mode_change(), bte_hci_set_sniff_mode()
 * @{ */
#define BTE_HCI_MODE_ACTIVE (uint8_t)0 /**< Active mode */
#define BTE_HCI_MODE_HOLD   (uint8_t)1 /**< Hold mode */
#define BTE_HCI_MODE_SNIFF  (uint8_t)2 /**< Sniff mode */
#define BTE_HCI_MODE_PARK   (uint8_t)3 /**< Park mode */
/** @} */

/**
 * @brief Set the sniff mode on a connection
 *
 * Set the sniff mode for the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param min_interval Minimum interval, in 0.625 of a millisecond
 * @param max_interval Maximum interval, in 0.625 of a millisecond
 * @param attempt_slots Number of slots, each being 1.25 milliseconds
 * @param timeout Timeout, in 1.25 of a millisecond
 * @param status_cb Status callback, can be \c NULL
 * @param userdata Client data to pass to \a status_cb
 *
 * @sa bte_hci_on_mode_change()
 */
void bte_hci_set_sniff_mode(BteHci *hci, BteConnHandle conn_handle,
                            uint16_t min_interval, uint16_t max_interval,
                            uint16_t attempt_slots, uint16_t timeout,
                            BteHciDoneCb status_cb, void *userdata);

/**
 * @brief Mode change event data
 *
 * @sa BteHciModeChangeCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** The current mode. See @ref BteHciModeG */
    uint8_t current_mode;
    /* Interval: for hold mode, the number of slots to wait; for sniff mode,
     * number of slots between anchor points. In both cases the slot time is
     * 0.625 milliseconds */
    uint16_t interval;
} BteHciModeChangeReply;
/**
 * @brief Callback for mode change events
 *
 * Invoked when the a mode change event occurs
 *
 * @param hci The HCI handle
 * @param reply Contains the current mode information
 * @param userdata The client data set on the BteClient
 *
 * @sa bte_hci_on_mode_change()
 */
typedef bool (*BteHciModeChangeCb)(BteHci *hci,
                                   const BteHciModeChangeReply *reply,
                                   void *userdata);
/**
 * @brief Watch for mode changes
 *
 * Install a callback to be notified on mode changes on the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param callback Function to be invoked when a mode change occurs
 */
void bte_hci_on_mode_change(BteHci *hci, BteConnHandle conn_handle,
                            BteHciModeChangeCb callback);

/** @defgroup BteHciLinkPolicySettingsG Link policy settings
 * @sa bte_hci_read_link_policy_settings(), bte_hci_write_link_policy_settings()
 * @{ */
#define BTE_HCI_LINK_POLICY_SETTINGS_DISABLE (uint8_t)0 /**< Disable */
/** Enable role switch */
#define BTE_HCI_LINK_POLICY_SETTINGS_ROLE_SW (uint8_t)1
#define BTE_HCI_LINK_POLICY_SETTINGS_HOLD    (uint8_t)2 /**< Enable hold */
#define BTE_HCI_LINK_POLICY_SETTINGS_SNIFF   (uint8_t)4 /**< Enable sniff */
#define BTE_HCI_LINK_POLICY_SETTINGS_PARK    (uint8_t)8 /**< Enable park */
/** @} */

/**
 * @brief Read link policy settings reply
 *
 * @sa BteHciReadLinkPolicySettingsCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** Link policy settings. See @ref BteHciLinkPolicySettingsG */
    BteHciLinkPolicySettings settings;
} BteHciReadLinkPolicySettingsReply;

/**
 * @brief Result callback for bte_hci_read_link_policy_settings()
 *
 * Invoked when the policy settings have been retrieved.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadLinkPolicySettingsCb)(
    BteHci *hci, const BteHciReadLinkPolicySettingsReply *reply,
    void *userdata);
/**
 * @brief Read the connection's link policy settings
 *
 * Request link policy settings for the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_link_policy_settings()
 */
void bte_hci_read_link_policy_settings(
    BteHci *hci, BteConnHandle conn_handle,
    BteHciReadLinkPolicySettingsCb callback, void *userdata);
/**
 * @brief Write the connection's link policy settings
 *
 * Set the link policy settings for the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param settings The link policy settings
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_link_policy_settings()
 */
void bte_hci_write_link_policy_settings(BteHci *hci,
                                        BteConnHandle conn_handle,
                                        BteHciLinkPolicySettings settings,
                                        BteHciDoneCb callback, void *userdata);

/**
 * @}
 */

/**
 * @defgroup ControllerBasebandG Controller & baseband commands
 * @{
 */

/**
 * @brief Event mask
 *
 * @sa BteHciEventMaskG
 */
typedef uint64_t BteHciEventMask;

/**
 * @brief Set the controller event mask
 *
 * Set the mask for the events to be received from the HCI controller. By
 * default, all events are enabled.
 *
 * @param hci The HCI handle
 * @param mask The event mask
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_set_event_filter(), BteHciEventMaskG
 */
void bte_hci_set_event_mask(BteHci *hci, BteHciEventMask mask,
                            BteHciDoneCb callback, void *userdata);

/**
 * @brief Reset the HCI controller
 *
 * Reset the HCI controller and the link manager.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 */
void bte_hci_reset(BteHci *hci, BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciEventFilterTypeG Event filter types
 * @{ */
/** Clear all filters */
#define BTE_HCI_EVENT_FILTER_TYPE_CLEAR            (uint8_t)0
/** Filter inquiry results (See @ref BteHciCondTypeInquiryG) */
#define BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT   (uint8_t)1
/** Filter connection setup (See @ref BteHciCondTypeConnSetupG) */
#define BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP (uint8_t)2
/** @} */

/** @defgroup BteHciCondTypeInquiryG Inquiry filters
 * @{ */
/** Report all devices */
#define BTE_HCI_COND_TYPE_INQUIRY_ALL     (uint8_t)0
/** Request a specific Class of Device (see @ref BteClassOfDevice) */
#define BTE_HCI_COND_TYPE_INQUIRY_COD     (uint8_t)1
/** Request a specific BD address (see @ref BteBdAddr) */
#define BTE_HCI_COND_TYPE_INQUIRY_ADDRESS (uint8_t)2
/** @} */

/** @defgroup BteHciCondTypeConnSetupG Connection setup filters
 * @{ */
/** Apply this filter to all connections */
#define BTE_HCI_COND_TYPE_CONN_SETUP_ALL     (uint8_t)0
/** Match the given Class of Device (see @ref BteClassOfDevice) */
#define BTE_HCI_COND_TYPE_CONN_SETUP_COD     (uint8_t)1
/** Match the given BD address (see @ref BteBdAddr) */
#define BTE_HCI_COND_TYPE_CONN_SETUP_ADDRESS (uint8_t)2
/** @} */

/** @defgroup BteHciCondValueConnSetupG Connection setup actions
 * @{ */
/** Do not accept the connection */
#define BTE_HCI_COND_VALUE_CONN_SETUP_AUTO_OFF   (uint8_t)1
/** Auto accept the connection with role switch disabled */
#define BTE_HCI_COND_VALUE_CONN_SETUP_SWITCH_OFF (uint8_t)2
/** Auto accept the connection with role switch enabled */
#define BTE_HCI_COND_VALUE_CONN_SETUP_SWITCH_ON  (uint8_t)3
/** @} */

/**
 * @brief Set the controller event filter
 *
 * Set the event filter. This function can be called multiple times, to
 * install more filters. By default no filters are installed, and incoming
 * connections are not automatically accepted.
 *
 * @param hci The HCI handle
 * @param filter_type The filter type, see @ref BteHciEventFilterTypeG
 * @param cond_type The filter condition type, one of:
 *        - @ref BteHciCondTypeInquiryG, if \a filter_type is @ref
 *          BTE_HCI_EVENT_FILTER_TYPE_INQUIRY_RESULT
 *        - @ref BteHciCondTypeConnSetupG, if \a filter_type is @ref
 *          BTE_HCI_EVENT_FILTER_TYPE_CONNECTION_SETUP
 *        - \c 0, if \a filter_type is @ref BTE_HCI_EVENT_FILTER_TYPE_CLEAR
 * @param filter_data The filter data, which depends on the previous two
 *        parameters:
 *        - If \a filter_type is @ref BTE_HCI_COND_TYPE_INQUIRY_COD
 *          -# A @ref BteClassOfDevice indicating the desired class of device
 *          -# A @ref BteClassOfDevice which works as a mask: if its bits are
 *             set to \c 1, the corresponding bits of the previous parameter
 *             will be checked
 *        - If \a filter_type is @ref BTE_HCI_COND_TYPE_INQUIRY_ADDRESS
 *          -# A @ref BteBdAddr indicating the desired BD address
 *        - If \a filter_type is @ref BTE_HCI_COND_TYPE_CONN_SETUP_ALL
 *          -# The action to take on incoming connections, see @ref
 *             BteHciCondValueConnSetupG
 *        - If \a filter_type is @ref BTE_HCI_COND_TYPE_CONN_SETUP_COD
 *          -# A @ref BteClassOfDevice indicating the desired class of device
 *          -# A @ref BteClassOfDevice which works as a mask: if its bits are
 *             set to \c 1, the corresponding bits of the previous parameter
 *             will be matched
 *          -# The action to take on incoming connections from devices matching
 *             this filter, see @ref BteHciCondValueConnSetupG
 *        - If \a filter_type is @ref BTE_HCI_COND_TYPE_CONN_SETUP_ADDRESS
 *          -# A @ref BteBdAddr indicating the desired BD address
 *          -# The action to take on incoming connections from devices matching
 *             this filter, see @ref BteHciCondValueConnSetupG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_set_event_mask()
 */
void bte_hci_set_event_filter(BteHci *hci, uint8_t filter_type,
                              uint8_t cond_type, const void *filter_data,
                              BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciPinTypeG PIN code types
 * @{ */
#define BTE_HCI_PIN_TYPE_VARIABLE (uint8_t)0 /**< Variable PIN */
#define BTE_HCI_PIN_TYPE_FIXED    (uint8_t)1 /**< Fixed PIN */
/** @} */

/**
 * @brief Read PIN type reply
 *
 * @sa BteHciReadPinTypeCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /* Type of PIN code. See @ref BteHciPinTypeG */
    uint8_t pin_type;
} BteHciReadPinTypeReply;

/**
 * @brief Result callback for bte_hci_read_pin_type()
 *
 * Invoked when the PIN type has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadPinTypeCb)(BteHci *hci,
                                    const BteHciReadPinTypeReply *reply,
                                    void *userdata);
/**
 * @brief Read PIN type
 *
 * Read the type of PIN type supported by the host.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_pin_type()
 */
void bte_hci_read_pin_type(
    BteHci *hci, BteHciReadPinTypeCb callback, void *userdata);
/**
 * @brief Write the PIN type
 *
 * Inform the link manager about the PIN type supported by the host.
 *
 * @param hci The HCI handle
 * @param pin_type The PIN type. See @ref BteHciPinTypeG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_pin_type()
 */
void bte_hci_write_pin_type(BteHci *hci, uint8_t pin_type,
                            BteHciDoneCb callback, void *userdata);

/**
 * @brief Link Key associated to a BD address
 *
 * @sa BteHciReadStoredLinkKeyReply
 */
typedef struct {
    /** BD address of the peer */
    BteBdAddr address;
    /** Link key */
    BteLinkKey key;
} BTE_PACKED BteHciStoredLinkKey;

/**
 * @brief Read stored link key reply
 *
 * @sa bte_hci_read_stored_link_key()
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Total number of keys that the controller can store. */
    uint16_t max_keys;
    /** Number of currently stored keys */
    uint16_t num_keys;
    /** Array of currently stored keys */
    const BteHciStoredLinkKey *stored_keys;
} BteHciReadStoredLinkKeyReply;

/**
 * @brief Result callback for bte_hci_read_stored_link_key()
 *
 * Invoked when the link key has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadStoredLinkKeyCb)(
    BteHci *hci, const BteHciReadStoredLinkKeyReply *reply, void *userdata);
/**
 * @brief Read the link keys stored in the HCI controller
 *
 * Get the list of keys stored in the HCI controller, as well as the maximum
 * number of keys that can be potentially stored.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device for which the link key should be
 *        retrieved, or \c NULL if all keys should be read.
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_stored_link_key()
 */
void bte_hci_read_stored_link_key(BteHci *hci, const BteBdAddr *address,
                                  BteHciReadStoredLinkKeyCb callback,
                                  void *userdata);

/**
 * @brief Write stored link key reply
 *
 * @sa BteHciWriteStoredLinkKeyCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Number of keys that have been written */
    uint8_t num_keys;
} BteHciWriteStoredLinkKeyReply;

/**
 * @brief Result callback for bte_hci_write_stored_link_key()
 *
 * Invoked when the link key has been stored.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciWriteStoredLinkKeyCb)(
    BteHci *hci, const BteHciWriteStoredLinkKeyReply *reply, void *userdata);
/**
 * @brief Store the given link keys into the HCI controller
 *
 * Add the given keys to the list of keys stored by the HCI controller.
 *
 * @param hci The HCI handle
 * @param num_keys Number of keys in the \a keys parameter
 * @param keys Array of keys to be stored
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_stored_link_key()
 */
void bte_hci_write_stored_link_key(BteHci *hci, int num_keys,
                                   const BteHciStoredLinkKey *keys,
                                   BteHciWriteStoredLinkKeyCb callback,
                                   void *userdata);

/**
 * @brief Delete stored link key reply
 *
 * @sa BteHciDeleteStoredLinkKeyCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Number of keys which were deleted */
    uint16_t num_keys;
} BteHciDeleteStoredLinkKeyReply;

/**
 * @brief Result callback for bte_hci_delete_stored_link_key()
 *
 * Invoked when the link key has been deleted.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciDeleteStoredLinkKeyCb)(
    BteHci *hci, const BteHciDeleteStoredLinkKeyReply *reply, void *userdata);
/**
 * @brief Remove link keys from the HCI controller
 *
 * Remove the link keys stored in the HCI controller.
 *
 * @param hci The HCI handle
 * @param address The BD address of the device for which the link key should be
 *        deleted, or \c NULL if all stored keys should be deleted.
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_stored_link_key()
 */
void bte_hci_delete_stored_link_key(BteHci *hci, const BteBdAddr *address,
                                    BteHciDeleteStoredLinkKeyCb callback,
                                    void *userdata);

/**
 * @brief Set the name of the local device
 *
 * Set the name which will be reported by the local device when queried.
 *
 * @param hci The HCI handle
 * @param name Null-terminated string holding the new name
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_local_name(), bte_hci_read_remote_name()
 */
void bte_hci_write_local_name(BteHci *hci, const char *name,
                              BteHciDoneCb callback, void *userdata);

/**
 * @brief Read local name reply
 *
 * @sa BteHciReadLocalNameCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Device name. Guaranteed to be null-terminated */
    char name[248 + 1];
} BteHciReadLocalNameReply;

/**
 * @brief Result callback for bte_hci_read_local_name()
 *
 * Invoked when the local name has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadLocalNameCb)(BteHci *hci,
                                      const BteHciReadLocalNameReply *reply,
                                      void *userdata);
/**
 * @brief Read the local device name
 *
 * Read the name of the local device.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_local_name()
 */
void bte_hci_read_local_name(BteHci *hci, BteHciReadLocalNameCb callback,
                             void *userdata);

/**
 * @brief Read page timeout reply
 *
 * @sa BteHciReadPageTimeoutCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Page timeout, unit is 0.625 of a millisecond */
    uint16_t page_timeout;
} BteHciReadPageTimeoutReply;

/**
 * @brief Result callback for bte_hci_read_page_timeout()
 *
 * Invoked when the page timeout has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadPageTimeoutCb)(
    BteHci *hci, const BteHciReadPageTimeoutReply *reply, void *userdata);
/**
 * @brief Read the page timeout
 *
 * Read the page timeout configuration parameter.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_page_timeout()
 */
void bte_hci_read_page_timeout(BteHci *hci, BteHciReadPageTimeoutCb callback,
                               void *userdata);
/**
 * @brief Write the page timeout
 *
 * Write the page timeout configuration parameter.
 *
 * @param hci The HCI handle
 * @param page_timeout The new page timeout, in 0.625 of a millisecond
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_page_timeout()
 */
void bte_hci_write_page_timeout(BteHci *hci, uint16_t page_timeout,
                                BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciScanEnableG Scan enable values
 * @{ */
#define BTE_HCI_SCAN_ENABLE_OFF      (uint8_t)0 /**< No scan enabled */
#define BTE_HCI_SCAN_ENABLE_INQUIRY  (uint8_t)1 /**< Inquiry scan */
#define BTE_HCI_SCAN_ENABLE_PAGE     (uint8_t)2 /**< Page scan */
#define BTE_HCI_SCAN_ENABLE_INQ_PAGE (uint8_t)3 /**< Inquiry and page scan */
/** @} */

/**
 * @brief Read scan enable reply
 *
 * @sa BteHciReadScanEnableCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Scan enable status, see @ref BteHciScanEnableG */
    uint8_t scan_enable;
} BteHciReadScanEnableReply;

/**
 * @brief Result callback for bte_hci_read_scan_enable()
 *
 * Invoked when the scan status has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadScanEnableCb)(BteHci *hci,
                                       const BteHciReadScanEnableReply *reply,
                                       void *userdata);
/**
 * @brief Read scan enable
 *
 * Read the scan enable configuration parameter.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_scan_enable()
 */
void bte_hci_read_scan_enable(
    BteHci *hci, BteHciReadScanEnableCb callback, void *userdata);
/**
 * @brief Write scan enable
 *
 * Write the scan enable configuration parameter.
 *
 * @param hci The HCI handle
 * @param scan_enable The scan enable parameter, see @ref BteHciScanEnableG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_scan_enable()
 */
void bte_hci_write_scan_enable(BteHci *hci, uint8_t scan_enable,
                               BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciAuthEnableG Authentication enable values
 * @{ */
/** Authentication not required */
#define BTE_HCI_AUTH_ENABLE_OFF (uint8_t)0
/** Authentication required on all connections */
#define BTE_HCI_AUTH_ENABLE_ON  (uint8_t)1
/** @} */

/**
 * @brief Read authentication enable reply
 *
 * @sa BteHciReadAuthEnableCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Authentication enable state, see @ref BteHciAuthEnableG */
    uint8_t auth_enable;
} BteHciReadAuthEnableReply;

/**
 * @brief Result callback for bte_hci_read_auth_enable()
 *
 * Invoked when the authentication status has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadAuthEnableCb)(BteHci *hci,
                                       const BteHciReadAuthEnableReply *reply,
                                       void *userdata);
/**
 * @brief Read authentication enable
 *
 * Read the authentication enable configuration parameter.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_auth_enable()
 */
void bte_hci_read_auth_enable(
    BteHci *hci, BteHciReadAuthEnableCb callback, void *userdata);
/**
 * @brief Write authentication enable
 *
 * Write the authentication enable configuration parameter.
 *
 * @param hci The HCI handle
 * @param auth_enable The new authentication enable value, see @ref
 *        BteHciAuthEnableG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_auth_enable()
 */
void bte_hci_write_auth_enable(BteHci *hci, uint8_t auth_enable,
                               BteHciDoneCb callback, void *userdata);

/**
 * @brief Read class of device reply
 *
 * @sa BteHciReadClassOfDeviceCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Class of device */
    BteClassOfDevice cod;
} BteHciReadClassOfDeviceReply;

/**
 * @brief Result callback for bte_hci_read_class_of_device()
 *
 * Invoked when the class of this device has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadClassOfDeviceCb)(
    BteHci *hci, const BteHciReadClassOfDeviceReply *reply, void *userdata);
/**
 * @brief Read the class of device
 *
 * Read the class of the device of the local device.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_class_of_device()
 */
void bte_hci_read_class_of_device(
    BteHci *hci, BteHciReadClassOfDeviceCb callback, void *userdata);
/**
 * @brief Write the class of device
 *
 * Write the class of the device of the local device.
 *
 * @param hci The HCI handle
 * @param cod The new class of device
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_class_of_device()
 */
void bte_hci_write_class_of_device(BteHci *hci, const BteClassOfDevice *cod,
                                   BteHciDoneCb callback, void *userdata);

/**
 * @brief Read automatic flush timeout reply
 *
 * @sa BteHciReadAutoFlushTimeoutCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** The flush timeout, in 0.625 of a millisecond */
    uint16_t flush_timeout;
} BteHciReadAutoFlushTimeoutReply;

/**
 * @brief Result callback for bte_hci_read_auto_flush_timeout()
 *
 * Invoked when the automatic flush timeout has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadAutoFlushTimeoutCb)(
    BteHci *hci, const BteHciReadAutoFlushTimeoutReply *reply, void *userdata);
/**
 * @brief Read the automatic flush timeout
 *
 * Read the automatic flush timeout on the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_auto_flush_timeout()
 */
void bte_hci_read_auto_flush_timeout(BteHci *hci,
                                     BteConnHandle conn_handle,
                                     BteHciReadAutoFlushTimeoutCb callback,
                                     void *userdata);
/**
 * @brief Write the automatic flush timeout
 *
 * Write the automatic flush timeout of the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param timeout The flush timeout, in 0.625 of a millisecond
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_auto_flush_timeout()
 */
void bte_hci_write_auto_flush_timeout(BteHci *hci,
                                      BteConnHandle conn_handle,
                                      uint16_t timeout,
                                      BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciCtrlToHostFlowControlG Controller to host flow control
 * @{ */
/** Flow control off in direction from controller to host. Default. */
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_OFF      (uint8_t)0
/** Flow control on for HCI ACL data packets and off for HCI synchronous
 * data packets in direction from controller to host. */
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_ACL      (uint8_t)1
/** Flow control off for HCI ACL data packets and on for HCI synchronous
 * data packets in direction from controller to host. */
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_SYNC     (uint8_t)2
/** Flow control on both for HCI ACL data packets and HCI synchronous
 * data packets in direction from controller to host. */
#define BTE_HCI_CTRL_TO_HOST_FLOW_CONTROL_ACL_SYNC (uint8_t)3
/** @} */

/**
 * @brief Set the controller to host flow control
 *
 * Set the controller to host flow control configuration parameter.
 *
 * @param hci The HCI handle
 * @param enable Type of flow control, see @ref BteHciCtrlToHostFlowControlG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 */
void bte_hci_set_ctrl_to_host_flow_control(
    BteHci *hci, uint8_t enable, BteHciDoneCb callback, void *userdata);

/**
 * @brief Set the host buffer sizes
 *
 * Inform the HCI controller about the buffer sizes which the host is capable
 * of handling.
 *
 * @param hci The HCI handle
 * @param acl_packet_len Maximum length (in octets) of the data portion of each
 *        HCI ACL data packet that the host is able to accept
 * @param acl_packets Total number of HCI ACL data packets that can be stored
 *        in the data buffers of the host
 * @param sync_packet_len Maximum length (in octets) of the data portion of
 *        each HCI synchronous data packet that the host is able to accept
 * @param sync_packets Total number of HCI synchronous data packets that can be
 *        stored in the data buffers of the host
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_buffer_size()
 */
void bte_hci_set_host_buffer_size(BteHci *hci,
                                  uint16_t acl_packet_len,
                                  uint16_t acl_packets,
                                  uint8_t sync_packet_len,
                                  uint16_t sync_packets,
                                  BteHciDoneCb callback, void *userdata);

/**
 * @brief Read current IAC LAP reply
 *
 * @sa BteHciReadCurrentIacLapCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Number of current LAPs */
    uint8_t num_laps;
    /** Array of LAPs */
    const BteLap *laps;
} BteHciReadCurrentIacLapReply;

/**
 * @brief Result callback for bte_hci_read_current_iac_lap()
 *
 * Invoked when the current IAC LAP has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadCurrentIacLapCb)(
    BteHci *hci, const BteHciReadCurrentIacLapReply *reply, void *userdata);
/**
 * @brief Read current IAC LAP
 *
 * Read the LAP(s) used to create the Inquiry Access Codes (IAC) that the local
 * Bluetooth device is simultaneously scanning for during inquiry scans.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_current_iac_lap()
 */
void bte_hci_read_current_iac_lap(
    BteHci *hci, BteHciReadCurrentIacLapCb callback, void *userdata);
/**
 * @brief Write the current IAC LAP
 *
 * Write the LAP(s) used to create the Inquiry Access Codes (IAC) that the local
 * Bluetooth device is simultaneously scanning for during inquiry scans.
 *
 * @param hci The HCI handle
 * @param num_laps Number of elements in the \a laps array
 * @param laps Array of IAC LAPs
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_current_iac_lap()
 */
void bte_hci_write_current_iac_lap(BteHci *hci,
                                   uint8_t num_laps, const BteLap *laps,
                                   BteHciDoneCb callback, void *userdata);

/**
 * @brief Write the PIN type
 *
 * This command is used by the host to indicate to the controller the number of
 * HCI data packets that have been completed for each connection handle since
 * the previous time this command was sent to the controller.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param num_packets Number of completed packets
 */
void bte_hci_host_num_comp_packets(BteHci *hci,
                                   BteConnHandle conn_handle,
                                   uint16_t num_packets);

/**
 * @brief Read link supervision timeout reply
 *
 * @sa BteHciReadLinkSvTimeoutCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Connection handle */
    BteConnHandle conn_handle;
    /** Supervision timeout, in 0.625 of a millisecond */
    uint16_t sv_timeout;
} BteHciReadLinkSvTimeoutReply;

/**
 * @brief Result callback for bte_hci_read_link_sv_timeout()
 *
 * Invoked when the link supervision timeout has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadLinkSvTimeoutCb)(
    BteHci *hci, const BteHciReadLinkSvTimeoutReply *reply, void *userdata);
/**
 * @brief Read the supervision timeout
 *
 * Read the supervision timeout on the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_link_sv_timeout()
 */
void bte_hci_read_link_sv_timeout(
    BteHci *hci, BteConnHandle conn_handle,
    BteHciReadLinkSvTimeoutCb callback, void *userdata);
/**
 * @brief Write the supervision timeout
 *
 * Write the supervision timeout of the given connection.
 *
 * @param hci The HCI handle
 * @param conn_handle The connection handle
 * @param timeout The supervision timeout, in 0.625 of a millisecond
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_link_sv_timeout()
 */
void bte_hci_write_link_sv_timeout(BteHci *hci,
                                   BteConnHandle conn_handle,
                                   uint16_t timeout,
                                   BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciInquiryScanTypeG Inquiry scan types
 * @{ */
#define BTE_HCI_INQUIRY_SCAN_TYPE_STANDARD   (uint8_t)0 /**< Standard scan */
#define BTE_HCI_INQUIRY_SCAN_TYPE_INTERLACED (uint8_t)1 /**< Interlaced scan */
/** @} */

/**
 * @brief Read inquiry scan type reply
 *
 * @sa BteHciReadInquiryScanTypeCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Inquiry scan type, see @ref BteHciInquiryScanTypeG */
    uint8_t inquiry_scan_type;
} BteHciReadInquiryScanTypeReply;

/**
 * @brief Result callback for bte_hci_read_inquiry_scan_type()
 *
 * Invoked when the inquiry scan type has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadInquiryScanTypeCb)(
    BteHci *hci, const BteHciReadInquiryScanTypeReply *reply, void *userdata);
/**
 * @brief Read inquiry scan type
 *
 * Read the inquiry scan type.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_inquiry_scan_type()
 */
void bte_hci_read_inquiry_scan_type(
    BteHci *hci, BteHciReadInquiryScanTypeCb callback, void *userdata);
/**
 * @brief Write inquiry scan type
 *
 * Write the inquiry scan type.
 *
 * @param hci The HCI handle
 * @param inquiry_scan_type Inquiry scan type, see @ref BteHciInquiryScanTypeG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_inquiry_scan_type()
 */
void bte_hci_write_inquiry_scan_type(BteHci *hci, uint8_t inquiry_scan_type,
                                     BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciInquiryModeG Inquiry modes
 * @{ */
#define BTE_HCI_INQUIRY_MODE_STANDARD (uint8_t)0 /**< Standard inquiry */
#define BTE_HCI_INQUIRY_MODE_RSSI     (uint8_t)1 /**< Inquiry with RSSI */
#define BTE_HCI_INQUIRY_MODE_EXTENDED (uint8_t)2 /**< Extended inquiry (TODO)*/
/** @} */

/**
 * @brief Read inquiry mode reply
 *
 * @sa BteHciReadInquiryModeCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Inquiry mode, see @ref BteHciInquiryModeG */
    uint8_t inquiry_mode;
} BteHciReadInquiryModeReply;

/**
 * @brief Result callback for bte_hci_read_inquiry_mode()
 *
 * Invoked when the inquiry mode has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadInquiryModeCb)(
    BteHci *hci, const BteHciReadInquiryModeReply *reply, void *userdata);
/**
 * @brief Read inquiry mode
 *
 * Read the inquiry mode.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_inquiry_mode()
 */
void bte_hci_read_inquiry_mode(
    BteHci *hci, BteHciReadInquiryModeCb callback, void *userdata);
/**
 * @brief Write inquiry mode
 *
 * Write the inquiry mode.
 *
 * @param hci The HCI handle
 * @param inquiry_mode Inquiry mode, see @ref BteHciInquiryModeG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_inquiry_mode()
 */
void bte_hci_write_inquiry_mode(BteHci *hci, uint8_t inquiry_mode,
                                BteHciDoneCb callback, void *userdata);

/** @defgroup BteHciPageScanTypeG Page scan types
 * @{ */
#define BTE_HCI_PAGE_SCAN_TYPE_STANDARD   (uint8_t)0 /**< Standard */
#define BTE_HCI_PAGE_SCAN_TYPE_INTERLACED (uint8_t)1 /**< Interlaced */
/** @} */

/**
 * @brief Read page scan type reply
 *
 * @sa BteHciReadPageScanTypeCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** Page scan type, see @ref BteHciPageScanTypeG */
    uint8_t page_scan_type;
} BteHciReadPageScanTypeReply;

/**
 * @brief Result callback for bte_hci_read_page_scan_type()
 *
 * Invoked when the page scan type has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadPageScanTypeCb)(
    BteHci *hci, const BteHciReadPageScanTypeReply *reply, void *userdata);
/**
 * @brief Read page scan type
 *
 * Read the page scan type configuration parameter.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_write_page_scan_type()
 */
void bte_hci_read_page_scan_type(
    BteHci *hci, BteHciReadPageScanTypeCb callback, void *userdata);
/**
 * @brief Write page scan type
 *
 * Write the page scan type configuration parameter.
 *
 * @param hci The HCI handle
 * @param page_scan_type The page scan type, see @ref BteHciPageScanTypeG
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_page_scan_type()
 */
void bte_hci_write_page_scan_type(BteHci *hci, uint8_t page_scan_type,
                                  BteHciDoneCb callback, void *userdata);

/**
 * @}
 */

/**
 * @defgroup InformationalG Informational parameters
 * @{
 */

/**
 * @brief Read local version reply
 *
 * @sa BteHciReadLocalVersionCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** HCI version, see the Assigned Numbers document */
    uint8_t hci_version;
    /** Revision of the Current HCI in the Bluetooth device. */
    uint16_t hci_revision;
    /** LMP version, see the Assigned Numbers document */
    uint8_t lmp_version;
    /** Manufacturer ID, see the Assigned Numbers document */
    uint16_t manufacturer;
    /** LMP subversion */
    uint16_t lmp_subversion;
} BteHciReadLocalVersionReply;

/**
 * @brief Result callback for bte_hci_read_local_version()
 *
 * Invoked when the local version has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadLocalVersionCb)(
    BteHci *hci, const BteHciReadLocalVersionReply *reply, void *userdata);
/**
 * @brief Read local version
 *
 * Read the version information from the local Bluetooth device.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_remote_version_info()
 */
void bte_hci_read_local_version(
    BteHci *hci, BteHciReadLocalVersionCb callback, void *userdata);

/**
 * @brief Read local features reply
 *
 * @sa BteHciReadLocalFeaturesCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** The bitmask of features supported by the local device */
    BteHciFeatures features;
} BteHciReadLocalFeaturesReply;

/**
 * @brief Result callback for bte_hci_read_local_features()
 *
 * Invoked when the local features have been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadLocalFeaturesCb)(
    BteHci *hci, const BteHciReadLocalFeaturesReply *reply, void *userdata);
/**
 * @brief Read local features
 *
 * Read the features bitmask supported from the local Bluetooth device.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_remote_features()
 */
void bte_hci_read_local_features(
    BteHci *hci, BteHciReadLocalFeaturesCb callback, void *userdata);

/**
 * @brief Read buffer size reply
 *
 * @sa BteHciReadBufferSizeCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** MTU for SCO packets */
    uint8_t sco_mtu;
    /** MTU for ACL packets */
    uint16_t acl_mtu;
    /** Total number of HCI SCO packets that can be stored in the data buffers
     * of the controller. */
    uint16_t sco_max_packets;
    /** Total number of HCI ACL data packets that can be stored in the data
     * buffers of the controller. */
    uint16_t acl_max_packets;
} BteHciReadBufferSizeReply;

/**
 * @brief Result callback for bte_hci_read_buffer_size()
 *
 * Invoked when the buffer sizes have been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadBufferSizeCb)(BteHci *hci,
                                       const BteHciReadBufferSizeReply *reply,
                                       void *userdata);
/**
 * @brief Read buffer sizes
 *
 * Read the buffer sizes supported from the local Bluetooth device.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_set_host_buffer_size()
 */
void bte_hci_read_buffer_size(
    BteHci *hci, BteHciReadBufferSizeCb callback, void *userdata);

/**
 * @brief Read BD address reply
 *
 * @sa BteHciReadBdAddrCb
 */
typedef struct {
    /** Error code for the operation. @sa BteErrorG */
    uint8_t status;
    /** BD address of the local device */
    BteBdAddr address;
} BteHciReadBdAddrReply;

/**
 * @brief Result callback for bte_hci_read_bd_addr()
 *
 * Invoked when the local address has been read.
 *
 * @param hci The HCI handle
 * @param reply Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciReadBdAddrCb)(BteHci *hci,
                                   const BteHciReadBdAddrReply *reply,
                                   void *userdata);
/**
 * @brief Read the address of the local device
 *
 * Read the BD address of the local device.
 *
 * @param hci The HCI handle
 * @param callback Function through which the reply will be delivered
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_read_local_name()
 */
void bte_hci_read_bd_addr(
    BteHci *hci, BteHciReadBdAddrCb callback, void *userdata);

/**
 * @}
 */

/**
 * @defgroup HciOtherG Other functions
 * @brief Vendor specific function
 * @{
 */

/**
 * @brief Result callback for bte_hci_vendor_command()
 *
 * Invoked when the vendor command has been executed.
 *
 * @param hci The HCI handle
 * @param reply_data Contains the result of the operation
 * @param userdata The same client data specified when invoking the command.
 */
typedef void (*BteHciVendorCommandCb)(BteHci *hci,
                                      BteBuffer *reply_data,
                                      void *userdata);
/**
 * @brief Issue a vendor command
 *
 * Send a vendor specific command to the HCI controller.
 *
 * @param hci The HCI handle
 * @param ocf The OpCode Command Field (OCF)
 * @param data The command data
 * @param len The length of the command data
 * @param callback Function to be invoked when the operation completes
 * @param userdata Client data to pass to \a callback
 *
 * @sa bte_hci_on_vendor_event()
 */
void bte_hci_vendor_command(BteHci *hci, uint16_t ocf,
                            const void *data, uint8_t len,
                            BteHciVendorCommandCb callback, void *userdata);

/**
 * @brief Callback for vendor specific events
 *
 * Invoked when the HCI controller emits a vendor specific event.
 *
 * @param hci The HCI handle
 * @param reply_data Mesage contents
 * @param userdata The client data set on the BteClient object
 *
 * @return \c true if the client has handled the event
 *
 * @sa bte_hci_on_vendor_event()
 */
typedef bool (*BteHciVendorEventCb)(BteHci *hci,
                                    BteBuffer *reply_data,
                                    void *userdata);
/**
 * @brief Watch for vendor events
 *
 * Register a callback to be invoked every time that a vendor specific event is
 * emitted by the HCI controller.
 *
 * @param hci The HCI handle
 * @param callback Function to be invoked when the event triggers
 */
void bte_hci_on_vendor_event(BteHci *hci, BteHciVendorEventCb callback);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BTE_HCI_H */
