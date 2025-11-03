#ifndef BTE_PLATFORM_DEFS_H
#define BTE_PLATFORM_DEFS_H

#ifdef __wii__
#  define BTE_BUFFER_ALIGNMENT_SIZE 32
#endif

#ifdef BTE_BUFFER_ALIGNMENT_SIZE
#  define BTE_BUFFER_ALIGN __attribute__((aligned(BTE_BUFFER_ALIGNMENT_SIZE)))
#else
#  define BTE_BUFFER_ALIGN
#endif

#endif /* BTE_PLATFORM_DEFS_H */
