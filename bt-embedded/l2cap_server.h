#ifndef BTE_L2CAP_SERVER_H
#define BTE_L2CAP_SERVER_H

#include "l2cap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup l2capG
 * @{
 */

/**
 * @defgroup l2capServerG L2CAP server API
 * @brief Listener for incoming L2CAP connections
 */

/**
 * @addtogroup l2capServerG
 * @{
 */

/**
 * @brief Handle to an L2CAP server
 *
 * An opaque handle to an L2CAP server. This is an object that can be used to
 * listen for incoming connections and establish an L2CAP channel on them.
 *
 * @sa bte_l2cap_server_new()
 */
typedef struct bte_l2cap_server_t BteL2capServer;

/**
 * @brief Register a new L2CAP server
 *
 * Create a new L2CAP server object listening for the given \a psm.
 *
 * @param client Handle to the main client object
 * @param psm The PSM (Protocol/Service Multiplexer)
 *
 * @return The handle to the newly created server
 *
 * @sa bte_l2cap_server_ref(), bte_l2cap_server_unref()
 */
BteL2capServer *bte_l2cap_server_new(BteClient *client, BteL2capPsm psm);

/**
 * @brief Add a reference to the L2CAP server object
 *
 * Mark the \a l2cap_server object as in use by incrementing its reference
 * count by one. The object will not be destroyed while the client has at least
 * one reference to it.
 *
 * @param l2cap_server The L2CAP server handle
 *
 * @return The same L2CAP server handle
 */
BteL2capServer *bte_l2cap_server_ref(BteL2capServer *l2cap_server);
/**
 * @brief Remove a reference to the L2CAP server object
 *
 * Decrement the reference count of \a l2cap_server by one. When the count
 * reaches zero, the object will be destroyed.
 *
 * @param l2cap_server The L2CAP server handle
 */
void bte_l2cap_server_unref(BteL2capServer *l2cap_server);

/**
 * @brief Get the client handle
 *
 * Get the BteClient handle managing this L2CAP server.
 *
 * @param l2cap_server The L2CAP server handle
 *
 * @return The client handle
 */
BteClient *bte_l2cap_server_get_client(BteL2capServer *l2cap_server);
/**
 * @brief Get the HCI handle
 *
 * Get a handle to the HCI controller.
 *
 * @param l2cap_server The L2CAP server handle
 *
 * @return The HCI handle
 */
BteHci *bte_l2cap_server_get_hci(BteL2capServer *l2cap_server);

/**
 * @brief Set the authentication required flag
 *
 * Instruct the L2CAP server to ask the client to authenticate immediately
 * after a connection is accepted.
 *
 * @param l2cap_server The L2CAP server handle
 * @param needs_auth \c true if the authentication is required, \c false
 *        otherwise
 *
 * @note It's allowed to call this function from within the
 *       BteL2capServerConnectionRequestCb callback.
 */
void bte_l2cap_server_set_needs_auth(BteL2capServer *l2cap_server,
                                     bool needs_auth);
/**
 * @brief Set the desired role for incoming connections
 *
 * Set the desired role to be set when accepting a connection.
 *
 * @param l2cap_server The L2CAP server handle
 * @param role The role we want to assume in the new connection
 *
 * @note It's allowed to call this function from within the
 *       BteL2capServerConnectionRequestCb callback.
 */
void bte_l2cap_server_set_role(BteL2capServer *l2cap_server, BteRole role);

/**
 * @brief Callback for established connections
 *
 * Invoked when a connection is established.
 *
 * @param l2cap_server The L2CAP server handle
 * @param l2cap The new L2CAP channel
 * @param userdata The client data set when the request was issued
 *
 * @note You should call bte_l2cap_ref() on the received \a l2cap object in
 *       order to continue to use it, otherwise it will get destroyed when the
 *       callback returns.
 *
 * @sa bte_l2cap_server_on_connected()
 */
typedef void (*BteL2capServerConnectedCb)(
    BteL2capServer *l2cap_server, BteL2cap *l2cap, void *userdata);

/**
 * @brief Watch for established connections
 *
 * Register a callback to be invoked when the a L2CAP connection gets
 * established.
 *
 * @param l2cap_server The L2CAP server handle
 * @param callback Function to be invoked when the event triggers
 * @param userdata Client data to pass to \a callback
 */
void bte_l2cap_server_on_connected(
    BteL2capServer *l2cap_server, BteL2capServerConnectedCb callback,
    void *userdata);

/**
 * @brief Callback for incoming connection requests
 *
 * Invoked when a connection is requested.
 *
 * @param l2cap_server The L2CAP server handle
 * @param address Address of the device we are connected to
 * @param cod The class of the remote device
 * @param userdata The client data set when the request was issued
 *
 * @return \c true if the connection should be accepted, \c false otherwise
 */
typedef bool (*BteL2capServerConnectionRequestCb)(
    BteL2capServer *l2cap_server, const BteBdAddr *address,
    const BteClassOfDevice *cod, void *userdata);

/**
 * Call this function if you want to control which incoming connections
 * should be accepted. By default, all connections are accepted.
 *
 * @param l2cap_server The L2CAP server handle
 * @param callback The function that will be called when an incoming
 *        connection is requested.
 * @param userdata Client data to pass to \a callback
 */
void bte_l2cap_server_on_connection_request(
    BteL2capServer *l2cap_server, BteL2capServerConnectionRequestCb callback,
    void *userdata);

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BTE_L2CAP_SERVER_H */
