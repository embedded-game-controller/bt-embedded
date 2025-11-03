#include "bte.h"

#include "backend.h"
#include "logging.h"

int bte_wait_events(void)
{
    bool wait_for_events = true;
    return _bte_backend.handle_events(wait_for_events);
}

int bte_handle_events(void)
{
    bool wait_for_events = false;
    return _bte_backend.handle_events(wait_for_events);
}
