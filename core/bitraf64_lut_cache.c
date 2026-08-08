#include "bitraf64_lut_cache.h"
#include <math.h>
#include <string.h>

/*
 * BITRAF64 Lookup Table (LUT) Cache Implementation (Phase 2)
 *
 * Precomputes all frequently-accessed values to eliminate
 * expensive computations (pow, multiply, modulo) in hot paths.
 */

static const double SQRT3_2 = 0.866025403784438646;

int bitraf64_lut_cache_init(bitraf64_lut_cache_t *cache) {
  if (!cache) return 0;

  /* Precompute spiral LUT: (√3/2)^n for n ∈ [0, 29] */
  for (uint32_t n = 0; n < BITRAF64_SPIRAL_LUT_SIZE; n++) {
    double spiral_value = pow(SQRT3_2, (double)n);
    uint64_t fixed_point = (uint64_t)(spiral_value * 65536.0);

    cache->spiral_lut[n].exponent = n;
    cache->spiral_lut[n].value_q48_16 = fixed_point;
  }

  /* Precompute layer LUT: (i+j+k) % 32 for all (i,j,k) */
  uint32_t idx = 0;
  for (uint32_t i = 0; i < 10; i++) {
    for (uint32_t j = 0; j < 10; j++) {
      for (uint32_t k = 0; k < 10; k++) {
        uint32_t layer = (i + j + k) % 32;
        cache->layer_lut[idx++] = layer;
      }
    }
  }

  /* Precompute value LUT: (i·j·k·f mod 60) × 2 for all (i,j,k,f) */
  idx = 0;
  for (uint32_t i = 0; i < 10; i++) {
    for (uint32_t j = 0; j < 10; j++) {
      for (uint32_t k = 0; k < 10; k++) {
        for (uint32_t f = 0; f < 6; f++) {
          uint32_t product = (i * j * k * f) % 60;
          uint16_t value = (uint16_t)(product * 2);
          cache->value_lut[idx++] = value;
        }
      }
    }
  }

  cache->valid = 1;
  cache->hits = 0;
  cache->misses = 0;

  return 1;
}

int bitraf64_lut_cache_free(bitraf64_lut_cache_t *cache) {
  if (!cache) return 0;
  memset(cache, 0, sizeof(*cache));
  return 1;
}

uint64_t bitraf64_lut_spiral_lookup(
    uint32_t exponent,
    const bitraf64_lut_cache_t *cache
) {
  if (!cache || !cache->valid) return 0;

  /* Bounds check: exponent should be ≤ 29 */
  if (exponent >= BITRAF64_SPIRAL_LUT_SIZE) {
    return 0;  /* Signal: out of range */
  }

  /* O(1) lookup */
  return cache->spiral_lut[exponent].value_q48_16;
}

uint32_t bitraf64_lut_layer_lookup(
    uint32_t i, uint32_t j, uint32_t k,
    const bitraf64_lut_cache_t *cache
) {
  if (!cache || !cache->valid) return 0;

  /* Bounds check */
  if (i >= 10 || j >= 10 || k >= 10) return 0;

  /* Compute flat index: i*100 + j*10 + k */
  uint32_t idx = i * 100 + j * 10 + k;

  if (idx >= BITRAF64_LAYER_LUT_SIZE) return 0;

  /* O(1) lookup */
  return cache->layer_lut[idx];
}

uint16_t bitraf64_lut_value_lookup(
    uint32_t i, uint32_t j, uint32_t k, uint32_t f,
    const bitraf64_lut_cache_t *cache
) {
  if (!cache || !cache->valid) return 0;

  /* Bounds check */
  if (i >= 10 || j >= 10 || k >= 10 || f >= 6) return 0;

  /* Compute flat index: (i*1000 + j*100 + k*10 + f) but optimized
   * Since f ∈ [0,5], we interleave: ((i*100 + j*10 + k) * 6) + f */
  uint32_t ijk_idx = i * 100 + j * 10 + k;
  uint32_t idx = ijk_idx * 6 + f;

  if (idx >= BITRAF64_VALUE_LUT_SIZE) return 0;

  /* O(1) lookup */
  return cache->value_lut[idx];
}

int bitraf64_lut_spiral_batch(
    const uint32_t *exponents,
    size_t count,
    uint64_t *out_results,
    const bitraf64_lut_cache_t *cache
) {
  if (!exponents || !out_results || !cache || !cache->valid) return 0;

  /* Batch spiral lookups with potential for vectorization */
  for (size_t i = 0; i < count; i++) {
    out_results[i] = bitraf64_lut_spiral_lookup(exponents[i], cache);
  }

  return 1;
}

int bitraf64_lut_get_stats(
    const bitraf64_lut_cache_t *cache,
    bitraf64_lut_stats_t *out_stats
) {
  if (!cache || !out_stats) return 0;

  out_stats->total_lookups = cache->hits + cache->misses;
  out_stats->cache_hits = cache->hits;
  out_stats->cache_misses = cache->misses;

  if (out_stats->total_lookups == 0) {
    out_stats->hit_rate = 0.0f;
  } else {
    out_stats->hit_rate = (float)cache->hits / (float)out_stats->total_lookups;
  }

  return 1;
}
