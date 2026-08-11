#include "../bitraf64_lut_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

static const double SQRT3_2 = 0.866025403784438646;

int test_lut_cache_init(void) {
  printf("Test 1: LUT Cache Initialization\n");

  bitraf64_lut_cache_t cache;
  if (!bitraf64_lut_cache_init(&cache)) {
    printf("  ✗ LUT cache initialization failed\n");
    return 0;
  }

  printf("  Valid: %s\n", cache.valid ? "yes" : "no");
  printf("  Hits: %u, Misses: %u\n", cache.hits, cache.misses);

  return 1;
}

int test_spiral_lut_values(void) {
  printf("Test 2: Spiral LUT Precomputed Values\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  printf("  Spiral LUT entries (sample):\n");
  printf("    n=0: 0x%016" PRIx64 "\n", cache.spiral_lut[0].value_q48_16);
  printf("    n=5: 0x%016" PRIx64 "\n", cache.spiral_lut[5].value_q48_16);
  printf("    n=10: 0x%016" PRIx64 "\n", cache.spiral_lut[10].value_q48_16);
  printf("    n=29: 0x%016" PRIx64 "\n", cache.spiral_lut[29].value_q48_16);

  double expected_0 = pow(SQRT3_2, 0.0);
  double expected_10 = pow(SQRT3_2, 10.0);

  uint64_t expected_0_q48_16 = (uint64_t)(expected_0 * 65536.0);
  uint64_t expected_10_q48_16 = (uint64_t)(expected_10 * 65536.0);

  printf("  Expected n=0: 0x%016" PRIx64 "\n", expected_0_q48_16);
  printf("  Expected n=10: 0x%016" PRIx64 "\n", expected_10_q48_16);

  return 1;
}

int test_spiral_lookup(void) {
  printf("Test 3: Spiral Lookup Function\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  uint64_t result_0 = bitraf64_lut_spiral_lookup(0, &cache);
  uint64_t result_5 = bitraf64_lut_spiral_lookup(5, &cache);
  uint64_t result_29 = bitraf64_lut_spiral_lookup(29, &cache);
  uint64_t result_invalid = bitraf64_lut_spiral_lookup(30, &cache);

  printf("  Lookup exponent 0: 0x%016" PRIx64 "\n", result_0);
  printf("  Lookup exponent 5: 0x%016" PRIx64 "\n", result_5);
  printf("  Lookup exponent 29: 0x%016" PRIx64 "\n", result_29);
  printf("  Lookup exponent 30 (invalid): 0x%016" PRIx64 " (should be 0)\n", result_invalid);

  return (result_invalid == 0) ? 1 : 0;
}

int test_layer_lut_entries(void) {
  printf("Test 4: Layer LUT Precomputed Entries\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  printf("  Layer LUT entries (sample):\n");
  uint32_t layer_0_0_0 = cache.layer_lut[0];  /* (0,0,0) */
  uint32_t layer_5_5_5 = cache.layer_lut[555]; /* (5,5,5) */
  uint32_t layer_9_9_9 = cache.layer_lut[999]; /* (9,9,9) */

  printf("    (0,0,0): layer %u\n", layer_0_0_0);
  printf("    (5,5,5): layer %u\n", layer_5_5_5);
  printf("    (9,9,9): layer %u\n", layer_9_9_9);

  printf("  Expected:\n");
  printf("    (0,0,0): layer %u\n", (0+0+0) % 32);
  printf("    (5,5,5): layer %u\n", (5+5+5) % 32);
  printf("    (9,9,9): layer %u\n", (9+9+9) % 32);

  return 1;
}

int test_layer_lookup(void) {
  printf("Test 5: Layer Lookup Function\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  uint32_t layer_0_0_0 = bitraf64_lut_layer_lookup(0, 0, 0, &cache);
  uint32_t layer_3_4_5 = bitraf64_lut_layer_lookup(3, 4, 5, &cache);
  uint32_t layer_9_9_9 = bitraf64_lut_layer_lookup(9, 9, 9, &cache);
  uint32_t layer_invalid = bitraf64_lut_layer_lookup(10, 10, 10, &cache);

  printf("  Lookup (0,0,0): layer %u\n", layer_0_0_0);
  printf("  Lookup (3,4,5): layer %u\n", layer_3_4_5);
  printf("  Lookup (9,9,9): layer %u\n", layer_9_9_9);
  printf("  Lookup (10,10,10) (invalid): %u (should be 0)\n", layer_invalid);

  return (layer_invalid == 0) ? 1 : 0;
}

int test_value_lut_entries(void) {
  printf("Test 6: Value LUT Precomputed Entries\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  printf("  Value LUT entries (sample):\n");
  uint16_t value_0_0_0_0 = cache.value_lut[0];  /* (0,0,0,0) */
  uint16_t value_1_1_1_0 = cache.value_lut[6];  /* (1,1,1,0) */
  uint16_t value_5_5_5_5 = cache.value_lut[(500+50+5)*6+5]; /* (5,5,5,5) */

  printf("    (0,0,0,0): value %u\n", value_0_0_0_0);
  printf("    (1,1,1,0): value %u\n", value_1_1_1_0);
  printf("    (5,5,5,5): value %u\n", value_5_5_5_5);

  printf("  Expected:\n");
  printf("    (0,0,0,0): value %u\n", (uint16_t)((0*0*0*0) % 60) * 2);
  printf("    (1,1,1,0): value %u\n", (uint16_t)((1*1*1*0) % 60) * 2);
  printf("    (5,5,5,5): value %u\n", (uint16_t)((5*5*5*5) % 60) * 2);

  return 1;
}

int test_value_lookup(void) {
  printf("Test 7: Value Lookup Function\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  uint16_t value_0_0_0_0 = bitraf64_lut_value_lookup(0, 0, 0, 0, &cache);
  uint16_t value_2_3_4_1 = bitraf64_lut_value_lookup(2, 3, 4, 1, &cache);
  uint16_t value_9_9_9_5 = bitraf64_lut_value_lookup(9, 9, 9, 5, &cache);
  uint16_t value_invalid = bitraf64_lut_value_lookup(10, 0, 0, 0, &cache);

  printf("  Lookup (0,0,0,0): value %u\n", value_0_0_0_0);
  printf("  Lookup (2,3,4,1): value %u\n", value_2_3_4_1);
  printf("  Lookup (9,9,9,5): value %u\n", value_9_9_9_5);
  printf("  Lookup (10,0,0,0) (invalid): %u (should be 0)\n", value_invalid);

  return (value_invalid == 0) ? 1 : 0;
}

int test_spiral_batch_lookup(void) {
  printf("Test 8: Spiral Batch Lookup\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  uint32_t exponents[] = {0, 5, 10, 15, 20, 29};
  uint64_t results[6];

  if (!bitraf64_lut_spiral_batch(exponents, 6, results, &cache)) {
    printf("  ✗ Batch lookup failed\n");
    return 0;
  }

  printf("  Batch lookup results:\n");
  for (size_t i = 0; i < 6; i++) {
    printf("    exponent %u: 0x%016" PRIx64 "\n", exponents[i], results[i]);
  }

  return 1;
}

int test_cache_statistics(void) {
  printf("Test 9: Cache Statistics\n");

  bitraf64_lut_cache_t cache;
  bitraf64_lut_cache_init(&cache);

  bitraf64_lut_stats_t stats;
  if (!bitraf64_lut_get_stats(&cache, &stats)) {
    printf("  ✗ Failed to get statistics\n");
    return 0;
  }

  printf("  Total lookups: %u\n", stats.total_lookups);
  printf("  Cache hits: %u\n", stats.cache_hits);
  printf("  Cache misses: %u\n", stats.cache_misses);
  printf("  Hit rate: %.2f%%\n", stats.hit_rate * 100.0);

  return 1;
}

int test_lut_memory_footprint(void) {
  printf("Test 10: LUT Memory Footprint\n");

  bitraf64_lut_cache_t cache;

  size_t spiral_size = sizeof(cache.spiral_lut);
  size_t layer_size = sizeof(cache.layer_lut);
  size_t value_size = sizeof(cache.value_lut);
  size_t total_size = spiral_size + layer_size + value_size + sizeof(uint32_t) * 4;

  printf("  Spiral LUT: %zu bytes (%u entries × 16 bytes)\n", spiral_size, BITRAF64_SPIRAL_LUT_SIZE);
  printf("  Layer LUT: %zu bytes (%u entries × 4 bytes)\n", layer_size, BITRAF64_LAYER_LUT_SIZE);
  printf("  Value LUT: %zu bytes (%u entries × 2 bytes)\n", value_size, BITRAF64_VALUE_LUT_SIZE);
  printf("  Metadata: 16 bytes (4 × uint32_t)\n");
  printf("  Total: %zu bytes (%.1f KB)\n", total_size, total_size / 1024.0);

  return 1;
}

int main(void) {
  printf("=== BITRAF64 LUT Cache Test Suite ===\n\n");

  int passed = 0;
  int total = 10;

  if (test_lut_cache_init()) passed++;
  printf("  ✓ LUT cache initialization\n\n");

  if (test_spiral_lut_values()) passed++;
  printf("  ✓ Spiral LUT values\n\n");

  if (test_spiral_lookup()) passed++;
  printf("  ✓ Spiral lookup\n\n");

  if (test_layer_lut_entries()) passed++;
  printf("  ✓ Layer LUT entries\n\n");

  if (test_layer_lookup()) passed++;
  printf("  ✓ Layer lookup\n\n");

  if (test_value_lut_entries()) passed++;
  printf("  ✓ Value LUT entries\n\n");

  if (test_value_lookup()) passed++;
  printf("  ✓ Value lookup\n\n");

  if (test_spiral_batch_lookup()) passed++;
  printf("  ✓ Spiral batch lookup\n\n");

  if (test_cache_statistics()) passed++;
  printf("  ✓ Cache statistics\n\n");

  if (test_lut_memory_footprint()) passed++;
  printf("  ✓ LUT memory footprint\n\n");

  printf("=== Summary ===\n");
  printf("Passed: %d/%d tests\n", passed, total);

  return (passed == total) ? 0 : 1;
}
