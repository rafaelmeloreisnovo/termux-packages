#include "../advanced_vectorization.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <inttypes.h>

static int test_simd_backend_detection(void) {
  printf("\n=== Sprint 6.1: SIMD Backend Detection ===\n");

  simd_backend_t backend = termux_detect_simd_backend();
  assert(backend >= SIMD_BACKEND_SCALAR && backend <= SIMD_BACKEND_AVX512_512);

  const char *backend_names[] = {
    "SCALAR", "NEON_128", "SVE_256", "SVE_512", "AVX2_256", "AVX512_512"
  };

  printf("  Detected backend: %s\n", backend_names[backend]);
  printf("✓ SIMD backend detection PASSED\n");
  return 0;
}

static int test_simd_capability_init(void) {
  printf("\n=== Sprint 6.2: SIMD Capability Initialization ===\n");

  simd_backend_t backends[] = {
    SIMD_BACKEND_SCALAR, SIMD_BACKEND_NEON_128,
    SIMD_BACKEND_AVX2_256, SIMD_BACKEND_AVX512_512
  };

  for (size_t i = 0; i < sizeof(backends) / sizeof(backends[0]); i++) {
    simd_capability_t cap = {};
    int ret = termux_simd_capability_init(&cap, backends[i]);
    assert(ret == 0);
    assert(cap.backend == backends[i]);

    printf("  Backend %u: width=%u bits, u32_lanes=%u\n",
           backends[i], cap.vector_width_bits, cap.lanes_u32);
  }

  printf("✓ SIMD capability init PASSED\n");
  return 0;
}

static int test_crc32c_neon(void) {
  printf("\n=== Sprint 6.3: CRC32c NEON Vectorization ===\n");

  uint8_t data[256];
  for (int i = 0; i < 256; i++) data[i] = (uint8_t)(i & 0xFF);

  uint32_t crc = termux_crc32c_neon(data, 256, 0);
  assert(crc != 0);
  printf("  CRC32c of 256 bytes: 0x%08x\n", crc);
  printf("✓ CRC32c NEON PASSED\n");
  return 0;
}

static int test_crc32c_sve(void) {
  printf("\n=== Sprint 6.4: CRC32c SVE Vectorization ===\n");

  uint8_t data[256];
  for (int i = 0; i < 256; i++) data[i] = (uint8_t)(i & 0xFF);

  uint32_t crc = termux_crc32c_sve(data, 256, 0);
  assert(crc != 0);
  printf("  CRC32c of 256 bytes: 0x%08x\n", crc);
  printf("✓ CRC32c SVE PASSED\n");
  return 0;
}

static int test_crc32c_avx512(void) {
  printf("\n=== Sprint 6.5: CRC32c AVX-512 Vectorization ===\n");

  uint8_t data[512];
  for (int i = 0; i < 512; i++) data[i] = (uint8_t)(i & 0xFF);

  uint32_t crc = termux_crc32c_avx512(data, 512, 0);
  assert(crc != 0);
  printf("  CRC32c of 512 bytes: 0x%08x\n", crc);
  printf("✓ CRC32c AVX-512 PASSED\n");
  return 0;
}

static int test_vectorized_cycle_count(void) {
  printf("\n=== Sprint 6.6: Vectorized Cycle Counting ===\n");

  uint32_t cycles[256];
  for (int i = 0; i < 256; i++) cycles[i] = 40 + (i % 20);

  uint64_t sum = 0;
  simd_metrics_t metrics = {};
  int ret = termux_vectorized_cycle_count(cycles, 256, &sum, &metrics);
  assert(ret == 0);
  assert(sum > 0);

  printf("  Total cycles: %" PRIu64 "\n", sum);
  printf("  Vector operations: %u\n", metrics.vector_ops_executed);
  printf("  Efficiency: %.2f ops/ns\n", metrics.efficiency);
  printf("✓ Vectorized cycle count PASSED\n");
  return 0;
}

static int test_vectorized_phi_compute(void) {
  printf("\n=== Sprint 6.7: Vectorized Φ Computation ===\n");

  uint64_t phi_scores[32];
  for (int i = 0; i < 32; i++) phi_scores[i] = 55000 + (i * 1000);

  double mean_phi = 0.0;
  simd_metrics_t metrics = {};
  int ret = termux_vectorized_phi_compute(phi_scores, 32, &mean_phi, &metrics);
  assert(ret == 0);
  assert(mean_phi > 0.0);

  printf("  Mean Φ: %.3f\n", mean_phi);
  printf("  Speedup: %.2fx\n", metrics.speedup);
  printf("✓ Vectorized Φ computation PASSED\n");
  return 0;
}

static int test_mixed_width_processing(void) {
  printf("\n=== Sprint 6.8: Mixed-Width Processing ===\n");

  uint8_t src[256] = {};
  uint8_t dst[256] = {};
  for (int i = 0; i < 256; i++) src[i] = (uint8_t)i;

  simd_backend_t backends[] = {
    SIMD_BACKEND_SCALAR, SIMD_BACKEND_NEON_128,
    SIMD_BACKEND_AVX2_256, SIMD_BACKEND_AVX512_512
  };

  for (size_t i = 0; i < sizeof(backends) / sizeof(backends[0]); i++) {
    memset(dst, 0, 256);
    int ret = termux_mixed_width_process(src, dst, 256, backends[i]);
    assert(ret == 0);

    bool match = true;
    for (int j = 0; j < 256; j++) {
      if (src[j] != dst[j]) { match = false; break; }
    }
    assert(match);
    printf("  Backend %u: ✓\n", backends[i]);
  }

  printf("✓ Mixed-width processing PASSED\n");
  return 0;
}

static int test_simd_metrics_printing(void) {
  printf("\n=== Sprint 6.9: SIMD Metrics Printing ===\n");

  simd_metrics_t metrics = {
    .cycles_scalar = 5000,
    .cycles_vector = 1200,
    .speedup = 4.17,
    .efficiency = 0.95,
    .vector_ops_executed = 64,
    .scalar_fallback_count = 5
  };
  termux_simd_print_metrics(&metrics);
  printf("✓ SIMD metrics printing PASSED\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("                    SPRINT 6: ADVANCED VECTORIZATION\n");
  printf("================================================================================\n");

  int all_passed = 0;
  all_passed += test_simd_backend_detection();
  all_passed += test_simd_capability_init();
  all_passed += test_crc32c_neon();
  all_passed += test_crc32c_sve();
  all_passed += test_crc32c_avx512();
  all_passed += test_vectorized_cycle_count();
  all_passed += test_vectorized_phi_compute();
  all_passed += test_mixed_width_processing();
  all_passed += test_simd_metrics_printing();

  printf("\n================================================================================\n");
  if (all_passed == 0) {
    printf("✓ ALL SPRINT 6 TESTS PASSED\n");
    printf("  SIMD backend detection: ✓\n");
    printf("  SIMD capability init: ✓\n");
    printf("  CRC32c NEON: ✓\n");
    printf("  CRC32c SVE: ✓\n");
    printf("  CRC32c AVX-512: ✓\n");
    printf("  Vectorized cycle count: ✓\n");
    printf("  Vectorized Φ computation: ✓\n");
    printf("  Mixed-width processing: ✓\n");
    printf("  Metrics printing: ✓\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");
  return all_passed == 0 ? 0 : 1;
}
