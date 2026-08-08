#ifndef TERMUX_COHERENCE_TUNING_H
#define TERMUX_COHERENCE_TUNING_H

#include <stdint.h>
#include <stddef.h>

#define TERMUX_L1_CACHE_SIZE 32768
#define TERMUX_L1_CACHE_LINE 64
#define TERMUX_L2_CACHE_SIZE 524288

typedef struct {
  uint64_t l1_accesses;
  uint64_t l1_misses;
  uint64_t l2_accesses;
  uint64_t l2_misses;
  uint64_t memory_ops;
  double l1_miss_rate;
  double l2_miss_rate;
} cache_metrics_t;

typedef struct {
  uint32_t layer_id;
  uint64_t coherence_phi;
  double cache_locality;
  double memory_efficiency;
  uint32_t package_count;
  uint32_t packages[65];
} layer_coherence_t;

uint64_t termux_coherence_compute_enhanced(uint32_t phase, uint32_t arch_state,
                                          uint32_t cycle_count,
                                          const cache_metrics_t *cache);

int termux_cache_metrics_measure(cache_metrics_t *metrics);

double termux_cache_locality_score(const cache_metrics_t *metrics);

double termux_memory_efficiency_score(const cache_metrics_t *metrics);

int termux_layer_coherence_optimize(layer_coherence_t *layer,
                                   const cache_metrics_t *cache);

void termux_coherence_print_report(const layer_coherence_t *layers, uint32_t count);

#endif
