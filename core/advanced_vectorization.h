#ifndef TERMUX_ADVANCED_VECTORIZATION_H
#define TERMUX_ADVANCED_VECTORIZATION_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
  SIMD_BACKEND_SCALAR = 0,
  SIMD_BACKEND_NEON_128 = 1,
  SIMD_BACKEND_SVE_256 = 2,
  SIMD_BACKEND_SVE_512 = 3,
  SIMD_BACKEND_AVX2_256 = 4,
  SIMD_BACKEND_AVX512_512 = 5
} simd_backend_t;

typedef struct {
  simd_backend_t backend;
  uint32_t vector_width_bits;
  uint32_t lanes_u8;
  uint32_t lanes_u32;
  uint32_t lanes_u64;
  uint8_t has_dotprod;
  uint8_t has_reduce;
  uint8_t has_crypto;
  uint8_t _pad;
} simd_capability_t;

typedef struct {
  uint64_t cycles_scalar;
  uint64_t cycles_vector;
  double speedup;
  double efficiency;
  uint32_t vector_ops_executed;
  uint32_t scalar_fallback_count;
} simd_metrics_t;

simd_backend_t termux_detect_simd_backend(void);

int termux_simd_capability_init(simd_capability_t *cap, simd_backend_t backend);

uint32_t termux_crc32c_neon(const uint8_t *data, size_t len, uint32_t crc);

uint32_t termux_crc32c_sve(const uint8_t *data, size_t len, uint32_t crc);

uint32_t termux_crc32c_avx512(const uint8_t *data, size_t len, uint32_t crc);

int termux_vectorized_cycle_count(const uint32_t *cycles, uint32_t count,
                                 uint64_t *sum, simd_metrics_t *metrics);

int termux_vectorized_phi_compute(const uint64_t *phi_scores, uint32_t count,
                                 double *mean_phi, simd_metrics_t *metrics);

int termux_mixed_width_process(const uint8_t *src, uint8_t *dst, size_t len,
                              simd_backend_t backend);

void termux_simd_print_metrics(const simd_metrics_t *metrics);

#endif
