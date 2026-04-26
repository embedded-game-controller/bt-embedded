#ifndef BTE_CLIENT_H
#define BTE_CLIENT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register a new client
 *
 * Create a new client object. Several clients might exist in the same process,
 * and bt-embedded will use this handle to tell them apart and deliver the
 * events to the correct client.
 *
 * @return The handle to the newly created client
 *
 * @sa bte_client_ref(), bte_client_unref()
 */
BteClient *bte_client_new(void);
/**
 * @brief Add a reference to the client
 *
 * Increment the reference count on the \a client object. The object will not
 * be destroyed while the client has at least one reference to it.
 *
 * @param client The client handle
 *
 * @return The same client handle
 */
BteClient *bte_client_ref(BteClient *client);
/**
 * @brief Drops a reference to the client
 *
 * Decrement the reference count of \a client by one. When the count reaches
 * zero, the object will be destroyed.
 *
 * @param client The client handle
 */
void bte_client_unref(BteClient *client);

/**
 * @brief Associate some user data to the client
 *
 * Associate the \a client object to \a userdata. This can be any arbitrary
 * data pointer. The bt-embedded library will not touch this memory.
 * It can be retrieved at any moment with bte_client_get_userdata().
 *
 * @param client The client handle
 * @param userdata The user data pointer
 */
void bte_client_set_userdata(BteClient *client, void *userdata);
/**
 * @brief Get the user data pointer
 *
 * Get the user data pointer which was set on the \a client object.
 *
 * @param client The client handle
 *
 * @return The user data pointer
 */
void *bte_client_get_userdata(BteClient *client);

#ifdef __cplusplus
}
#endif

#endif /* BTE_CLIENT_H */
