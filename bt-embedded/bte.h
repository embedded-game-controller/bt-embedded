#ifndef BTE_BTE_H
#define BTE_BTE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wait for events and invoke callbacks. This function blocks for a maximum of
 * timeout_ms if no events occur. Passing 0 as timeout_ms means that this
 * function can sleep forever. */
int bte_wait_events(uint32_t timeout_ms);

/* Fetch events and invoke callbacks. This function returns immediately if
 * there are no events to deliver. */
int bte_handle_events(void);

#ifdef __cplusplus
}
#endif

#endif /* BTE_BTE_H */
