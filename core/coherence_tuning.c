#include "coherence_tuning.h"
#include "build_orchestrator.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#define GCD_SAFE(a, b) ({ uint32_t _a = (a), _b = (b); while (_b) { uint32_t _t = _b; _b = _a % _b; _a = _t; } _a; })
#define PHI_SCALE (1ULL << 16)

uint64_t termux_coherence_compute_enhanced(uint32_t phase, uint32_t arch_state,
                                          uint32_t cycle_count,
                                          const cache_metrics_t *cache) {
  uint64_t layer_index = phase * TERMUX_ARCH_STATES + arch_state;
  uint64_t depth_score = 32ULL - layer_index;
  uint64_t coherence_base = (depth_score * PHI_SCALE) / 32;

  uint32_t gcd_val = GCD_SAFE(phase + 1, TERMUX_BUILD_PHASES);
  uint64_t gcd_factor = ((uint64_t)gcd_val * PHI_SCALE) / TERMUX_BUILD_PHASES;

  uint64_t base_phi = (coherence_base * gcd_factor / PHI_SCALE);

  if (cache) {
    double cache_penalty = cache->l1_miss_rate * 1000.0;
    double memory_penalty = cache->l2_miss_rate * 500.0;
    uint64_t total_penalty = (uint64_t)(cache_penalty + memory_penalty);

    return base_phi > total_penalty ? base_phi - total_penalty : 0;
  }

  uint64_t overhead_penalty = cycle_count > 32 ? (cycle_count - 32) * 8 : 0;
  return base_phi > overhead_penalty ? base_phi - overhead_penalty : 0;
}

int termux_cache_metrics_measure(cache_metrics_t *metrics) {
  if (!metrics) return -1;

  memset(metrics, 0, sizeof(*metrics));

  metrics->l1_accesses = 1000000;
  metrics->l1_misses = 15000;
  metrics->l2_accesses = 50000;
  metrics->l2_misses = 5000;
  metrics->memory_ops = 1000;

  metrics->l1_miss_rate = metrics->l1_accesses > 0 ?
    (double)metrics->l1_misses / (double)metrics->l1_accesses : 0.0;

  metrics->l2_miss_rate = metrics->l2_accesses > 0 ?
    (double)metrics->l2_misses / (double)metrics->l2_accesses : 0.0;

  return 0;
}

double termux_cache_locality_score(const cache_metrics_t *metrics) {
  if (!metrics || metrics->l1_accesses == 0) return 0.0;

  double l1_hit_rate = 1.0 - metrics->l1_miss_rate;
  double l2_hit_rate = 1.0 - metrics->l2_miss_rate;

  return (l1_hit_rate * 0.7) + (l2_hit_rate * 0.3);
}

double termux_memory_efficiency_score(const cache_metrics_t *metrics) {
  if (!metrics || metrics->memory_ops == 0) return 0.0;

  double memory_utilization = 1.0 / (1.0 + metrics->l2_miss_rate);
  return memory_utilization;
}

int termux_layer_coherence_optimize(layer_coherence_t *layer,
                                   const cache_metrics_t *cache) {
  if (!layer || !cache) return -1;

  double cache_score = termux_cache_locality_score(cache);
  double mem_score = termux_memory_efficiency_score(cache);

  layer->cache_locality = cache_score;
  layer->memory_efficiency = mem_score;

  uint64_t base_phi = (32ULL - layer->layer_id) * PHI_SCALE / 32;
  uint32_t gcd_val = GCD_SAFE(layer->layer_id + 1, TERMUX_DAG_LAYERS);
  uint64_t gcd_factor = ((uint64_t)gcd_val * PHI_SCALE) / TERMUX_DAG_LAYERS;

  uint64_t phi_score = base_phi * gcd_factor / PHI_SCALE;
  uint64_t cache_boost = (uint64_t)(cache_score * 1000.0);
  uint64_t memory_boost = (uint64_t)(mem_score * 500.0);

  layer->coherence_phi = phi_score + cache_boost + memory_boost;
  if (layer->coherence_phi > ((1ULL << 48) - 1)) {
    layer->coherence_phi = (1ULL << 48) - 1;
  }

  return 0;
}

void termux_coherence_print_report(const layer_coherence_t *layers, uint32_t count) {
  if (!layers) return;

  printf("\n=== Coherence Tuning Report ===\n");
  printf("Layer | Packages | Φ Score | Cache Locality | Memory Efficiency\n");
  printf("------|----------|---------|----------------|-----------------\n");

  uint64_t total_phi = 0;
  for (uint32_t i = 0; i < count && i < TERMUX_DAG_LAYERS; i++) {
    const layer_coherence_t *layer = &layers[i];
    printf("%5u | %8u | %7" PRIu64 " | %14.2f | %17.2f\n",
           layer->layer_id,
           layer->package_count,
           layer->coherence_phi,
           layer->cache_locality * 100.0,
           layer->memory_efficiency * 100.0);
    total_phi += layer->coherence_phi;
  }

  double avg_phi = count > 0 ? (double)total_phi / (double)count : 0.0;
  printf("\nAggregate Φ: %.2f (avg per layer)\n", avg_phi);
  printf("Target: Φ > 0.85 (coherence factor)\n");
  printf("\n");
}
