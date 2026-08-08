#include "alloc_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Global Pool State
 * ============================================================================ */

static memory_pool_t pool_256b;
static memory_pool_t pool_1kb;
static memory_pool_t pool_4kb;

static uint8_t pool_256b_data[POOL_256B_COUNT * 256];
static uint8_t pool_1kb_data[POOL_1KB_COUNT * 1024];
static uint8_t pool_4kb_data[POOL_4KB_COUNT * 4096];

static pool_block_t pool_256b_blocks[POOL_256B_COUNT];
static pool_block_t pool_1kb_blocks[POOL_1KB_COUNT];
static pool_block_t pool_4kb_blocks[POOL_4KB_COUNT];

static int pools_initialized = 0;

/* ============================================================================
 * Pool Initialization
 * ============================================================================ */

static int pool_init_single(memory_pool_t *pool, uint8_t *data, pool_block_t *blocks,
                            uint32_t block_size, uint32_t block_count) {
  if (!pool || !data || !blocks) return -1;

  memset(pool, 0, sizeof(*pool));
  pool->block_size = block_size;
  pool->total_blocks = block_count;
  pool->free_blocks = block_count;

  /* Initialize free list */
  for (uint32_t i = 0; i < block_count; i++) {
    blocks[i].data = data + (i * block_size);
    blocks[i].size = block_size;
    blocks[i].is_allocated = 0;
    blocks[i].next = (i < block_count - 1) ? &blocks[i + 1] : NULL;
  }

  pool->free_list = &blocks[0];
  pool->allocated_list = NULL;

  return 0;
}

int pool_system_init(void) {
  if (pools_initialized) return 0;

  if (pool_init_single(&pool_256b, pool_256b_data, pool_256b_blocks, 256, POOL_256B_COUNT) != 0)
    return -1;
  if (pool_init_single(&pool_1kb, pool_1kb_data, pool_1kb_blocks, 1024, POOL_1KB_COUNT) != 0)
    return -2;
  if (pool_init_single(&pool_4kb, pool_4kb_data, pool_4kb_blocks, 4096, POOL_4KB_COUNT) != 0)
    return -3;

  pools_initialized = 1;
  return 0;
}

/* ============================================================================
 * Allocation & Deallocation
 * ============================================================================ */

static void *pool_alloc_from_pool(memory_pool_t *pool) {
  if (!pool || !pool->free_list) {
    pool->miss_count++;
    return NULL;  /* No free blocks */
  }

  /* Get first free block */
  pool_block_t *block = pool->free_list;
  pool->free_list = block->next;

  block->is_allocated = 1;
  block->next = pool->allocated_list;
  pool->allocated_list = block;

  pool->free_blocks--;
  pool->alloc_count++;
  pool->hit_count++;

  /* Track peak usage */
  uint32_t used = pool->total_blocks - pool->free_blocks;
  if (used > pool->peak_usage) {
    pool->peak_usage = used;
  }

  return block->data;
}

void *pool_alloc(size_t size) {
  if (!pools_initialized) {
    pool_system_init();
  }

  /* Route to appropriate pool based on size */
  if (size <= 256) {
    return pool_alloc_from_pool(&pool_256b);
  } else if (size <= 1024) {
    return pool_alloc_from_pool(&pool_1kb);
  } else if (size <= 4096) {
    return pool_alloc_from_pool(&pool_4kb);
  } else {
    /* Size too large for pools, fallback to malloc */
    pool_256b.miss_count++;
    return malloc(size);
  }
}

static int pool_free_from_pool(memory_pool_t *pool, void *ptr) {
  if (!pool || !ptr) return -1;

  /* Find block in allocated list */
  pool_block_t *prev = NULL;
  pool_block_t *block = pool->allocated_list;

  while (block) {
    if (block->data == ptr) {
      /* Unlink from allocated list */
      if (prev) {
        prev->next = block->next;
      } else {
        pool->allocated_list = block->next;
      }

      /* Add to free list */
      block->next = pool->free_list;
      pool->free_list = block;
      block->is_allocated = 0;

      pool->free_blocks++;
      pool->free_count++;

      return 0;
    }

    prev = block;
    block = block->next;
  }

  return -2;  /* Block not found in this pool */
}

int pool_free(void *ptr, size_t size) {
  if (!ptr) return -1;

  /* Try to find in appropriate pool */
  if (size <= 256) {
    if (pool_free_from_pool(&pool_256b, ptr) == 0) return 0;
  } else if (size <= 1024) {
    if (pool_free_from_pool(&pool_1kb, ptr) == 0) return 0;
  } else if (size <= 4096) {
    if (pool_free_from_pool(&pool_4kb, ptr) == 0) return 0;
  }

  /* Not found in pools, assume it came from malloc */
  free(ptr);
  return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int pool_get_stats(pool_stats_t *stats) {
  if (!stats) return -1;

  if (!pools_initialized) {
    memset(stats, 0, sizeof(*stats));
    return -2;
  }

  memset(stats, 0, sizeof(*stats));
  stats->pools_initialized = 3;

  uint64_t total_alloc = pool_256b.alloc_count + pool_1kb.alloc_count + pool_4kb.alloc_count;
  uint64_t total_hit = pool_256b.hit_count + pool_1kb.hit_count + pool_4kb.hit_count;

  stats->total_allocated = (uint32_t)(
    (pool_256b.total_blocks - pool_256b.free_blocks) * 256 +
    (pool_1kb.total_blocks - pool_1kb.free_blocks) * 1024 +
    (pool_4kb.total_blocks - pool_4kb.free_blocks) * 4096
  );

  stats->total_available = POOL_256B_COUNT * 256 + POOL_1KB_COUNT * 1024 + POOL_4KB_COUNT * 4096;

  /* Hit rates */
  if (pool_256b.alloc_count > 0) {
    stats->hit_rate_256b = (double)pool_256b.hit_count / pool_256b.alloc_count;
  }
  if (pool_1kb.alloc_count > 0) {
    stats->hit_rate_1kb = (double)pool_1kb.hit_count / pool_1kb.alloc_count;
  }
  if (pool_4kb.alloc_count > 0) {
    stats->hit_rate_4kb = (double)pool_4kb.hit_count / pool_4kb.alloc_count;
  }

  if (total_alloc > 0) {
    stats->overall_hit_rate = (double)total_hit / total_alloc;
  }

  return 0;
}

void pool_system_shutdown(void) {
  /* Pools use pre-allocated static memory, nothing to free */
  memset(&pool_256b, 0, sizeof(pool_256b));
  memset(&pool_1kb, 0, sizeof(pool_1kb));
  memset(&pool_4kb, 0, sizeof(pool_4kb));
  pools_initialized = 0;
}

/* ============================================================================
 * Instrumentation
 * ============================================================================ */

int pool_get_instrumentation(pool_instrumentation_t *instr) {
  if (!instr) return -1;

  memset(instr, 0, sizeof(*instr));

  instr->total_allocations = pool_256b.alloc_count + pool_1kb.alloc_count + pool_4kb.alloc_count;
  instr->total_frees = pool_256b.free_count + pool_1kb.free_count + pool_4kb.free_count;
  instr->pool_hits = pool_256b.hit_count + pool_1kb.hit_count + pool_4kb.hit_count;
  instr->pool_misses = pool_256b.miss_count + pool_1kb.miss_count + pool_4kb.miss_count;

  uint32_t max_usage_256b = pool_256b.total_blocks - pool_256b.free_blocks;
  uint32_t max_usage_1kb = pool_1kb.total_blocks - pool_1kb.free_blocks;
  uint32_t max_usage_4kb = pool_4kb.total_blocks - pool_4kb.free_blocks;

  instr->max_concurrent_allocations = max_usage_256b + max_usage_1kb + max_usage_4kb;

  uint32_t peak_bytes = pool_256b.peak_usage * 256 + pool_1kb.peak_usage * 1024 + pool_4kb.peak_usage * 4096;
  uint32_t total_capacity = POOL_256B_COUNT * 256 + POOL_1KB_COUNT * 1024 + POOL_4KB_COUNT * 4096;
  instr->average_block_utilization = (double)peak_bytes / total_capacity;

  return 0;
}

void pool_report(FILE *out) {
  if (!out) out = stdout;

  fprintf(out, "=== Memory Pool Report ===\n");

  pool_stats_t stats;
  if (pool_get_stats(&stats) == 0) {
    fprintf(out, "Pools initialized: %u\n", stats.pools_initialized);
    fprintf(out, "Total allocated: %u bytes\n", stats.total_allocated);
    fprintf(out, "Total available: %u bytes\n", stats.total_available);
    fprintf(out, "\nHit rates:\n");
    fprintf(out, "  256B pool: %.1f%%\n", stats.hit_rate_256b * 100);
    fprintf(out, "  1KB pool:  %.1f%%\n", stats.hit_rate_1kb * 100);
    fprintf(out, "  4KB pool:  %.1f%%\n", stats.hit_rate_4kb * 100);
    fprintf(out, "  Overall:   %.1f%%\n", stats.overall_hit_rate * 100);
  }

  pool_instrumentation_t instr;
  if (pool_get_instrumentation(&instr) == 0) {
    fprintf(out, "\nInstrumentation:\n");
    fprintf(out, "  Total allocations: %llu\n", (unsigned long long)instr.total_allocations);
    fprintf(out, "  Total frees: %llu\n", (unsigned long long)instr.total_frees);
    fprintf(out, "  Pool hits: %llu\n", (unsigned long long)instr.pool_hits);
    fprintf(out, "  Pool misses: %llu\n", (unsigned long long)instr.pool_misses);
    fprintf(out, "  Max concurrent: %u\n", instr.max_concurrent_allocations);
    fprintf(out, "  Peak utilization: %.1f%%\n", instr.average_block_utilization * 100);
  }

  fprintf(out, "\n");
}
