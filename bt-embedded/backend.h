#ifndef BTE_BACKEND_H
#define BTE_BACKEND_H

#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bte_backend_t BteBackend;

/* Interface for platform-specific BT backends. */
struct bte_backend_t {
    int (*init)(void);

    int (*handle_events)(bool wait_for_events);

    int (*hci_send_command)(BteBuffer *buf);
    int (*hci_send_data)(BteBuffer *buf);

    int (*deinit)(void);
};

extern const BteBackend _bte_backend;

#ifdef __cplusplus
}
#endif

#endif
