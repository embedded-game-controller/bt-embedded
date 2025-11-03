#ifndef BTE_BTE_H
#define BTE_BTE_H

/* Wait for events and invoke callbacks. */
int bte_wait_events(void);

/* Fetch events and invoke callbacks. This function returns immediately if
 * there are no events to deliver. */
int bte_handle_events(void);

#endif /* BTE_BTE_H */
