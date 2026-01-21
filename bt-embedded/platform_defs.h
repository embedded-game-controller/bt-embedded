#ifndef BTE_PLATFORM_DEFS_H
#define BTE_PLATFORM_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __wii__
#  define BTE_BUFFER_ALIGNMENT_SIZE 32
#endif

#ifdef BTE_BUFFER_ALIGNMENT_SIZE
#  define BTE_BUFFER_ALIGN __attribute__((aligned(BTE_BUFFER_ALIGNMENT_SIZE)))
#else
#  define BTE_BUFFER_ALIGN
#endif

/* Definition of allocation functions */
#define bte_malloc(size) malloc(size)
#define bte_malloc_aligned(alignment, size) \
    memalign(alignment, ((size) + alignment - 1) & ~(alignment - 1))
#define bte_free free

#ifdef __cplusplus
}
#endif

#endif /* BTE_PLATFORM_DEFS_H */
