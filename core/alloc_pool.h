#ifndef ALLOC_POOL_H
#define ALLOC_POOL_H

/*
 * Phase 9.19: Zero-Alloc Memory Pooling
 * Pre-allocated buffer pools for hot path performance
 * Expected: 99% hit rate, zero malloc in critical section
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ============================================================================
 * Pool Configuration
 * ============================================================================ */

/* Pre-allocated pool sizes (tuned for Termux build system) */
#define POOL_256B_COUNT  64    /* 64 × 256B buffers = 16 KB */
#define POOL_1KB_COUNT   32    /* 32 × 1KB buffers = 32 KB */
#define POOL_4KB_COUNT   16    /* 16 × 4KB buffers = 64 KB */
/* Total: 112 KB pre-allocated, zero fragmentation */

/* ============================================================================
 * Pool Block Management
 * ============================================================================ */

typedef struct pool_block {
  struct pool_block *next;  /* Linked list for free blocks */
  uint8_t *data;
  size_t size;
  uint8_t is_allocated;
} pool_block_t;

typedef struct {
  pool_block_t *free_list;
  pool_block_t *allocated_list;
  uint32_t block_size;
  uint32_t total_blocks;
  uint32_t free_blocks;
  uint32_t peak_usage;
  uint64_t alloc_count;
  uint64_t free_count;
  uint64_t hit_count;
  uint64_t miss_count;
} memory_pool_t;

/* ============================================================================
 * Pool Operations
 * ============================================================================ */

/* Initialize all pools */
int pool_system_init(void);

/* Allocate from appropriate pool (returns NULL if all pools exhausted) */
void *pool_alloc(size_t size);

/* Free back to pool */
int pool_free(void *ptr, size_t size);

/* Get pool statistics */
typedef struct {
  uint32_t pools_initialized;
  uint32_t total_allocated;
  uint32_t total_available;
  double hit_rate_256b;
  double hit_rate_1kb;
  double hit_rate_4kb;
  double overall_hit_rate;
} pool_stats_t;

int pool_get_stats(pool_stats_t *stats);

/* Shutdown pools and free all memory */
void pool_system_shutdown(void);

/* ============================================================================
 * Utility Macros (for compile-time pool size hints)
 * ============================================================================ */

#define POOL_ALLOC_256B(size)  (((size) <= 256) ? pool_alloc(256) : NULL)
#define POOL_ALLOC_1KB(size)   (((size) <= 1024) ? pool_alloc(1024) : NULL)
#define POOL_ALLOC_4KB(size)   (((size) <= 4096) ? pool_alloc(4096) : NULL)

/* ============================================================================
 * Instrumentation
 * ============================================================================ */

typedef struct {
  uint64_t total_allocations;
  uint64_t total_frees;
  uint64_t pool_hits;
  uint64_t pool_misses;
  uint32_t max_concurrent_allocations;
  double average_block_utilization;
} pool_instrumentation_t;

/* Get detailed instrumentation data */
int pool_get_instrumentation(pool_instrumentation_t *instr);

/* Report pool performance */
void pool_report(FILE *out);

#endif  /* ALLOC_POOL_H */
