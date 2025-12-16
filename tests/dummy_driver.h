#ifndef BTE_TESTS_DUMMY_DRIVER_H
#define BTE_TESTS_DUMMY_DRIVER_H

#include "bt-embedded/driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for the tests */
void dummy_driver_set_acl_limits(uint16_t acl_mtu, uint16_t acl_max_packets);

#ifdef __cplusplus
}
#endif

#endif /* BTE_TESTS_DUMMY_DRIVER_H */
