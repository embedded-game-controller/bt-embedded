#ifndef BTE_LOGGING_H
#define BTE_LOGGING_H

#include <inttypes.h>
#include <stdio.h>

#define DEBUG 1

#define BTE_WARN(...) printf(__VA_ARGS__)
#define BTE_INFO(...) printf(__VA_ARGS__)
#if DEBUG
#define BTE_DEBUG(...) printf(__VA_ARGS__)
#else
#define BTE_DEBUG(...) (void)0
#endif

#endif /* BTE_LOGGING_H */
