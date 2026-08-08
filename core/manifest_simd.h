#ifndef MANIFEST_SIMD_H
#define MANIFEST_SIMD_H

/*
 * Phase 9.19: SIMD-Accelerated Manifest Validation
 * CRC32c computation with NEON (ARM) or AVX2 (x86) vectorization
 * Expected speedup: 3-6× vs scalar CRC32c
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __aarch64__
  #include <arm_neon.h>
  #define SIMD_AVAILABLE 1
  #define SIMD_BACKEND "NEON (ARM64)"
#elif defined(__x86_64__)
  #ifdef __AVX2__
    #include <immintrin.h>
    #define SIMD_AVAILABLE 1
    #define SIMD_BACKEND "AVX2 (x86_64)"
  #else
    #define SIMD_AVAILABLE 0
  #endif
#else
  #define SIMD_AVAILABLE 0
#endif

/* ============================================================================
 * CRC32c Polynomial: 0x1EDC6F41 (Castagnoli)
 * Used for fast validation of manifest entries and dependency lists
 * ============================================================================ */

typedef struct {
  uint32_t crc32c_value;
  uint32_t bytes_processed;
  uint8_t simd_enabled;
  const char *backend;
} crc32c_context_t;

/* Initialize CRC32c context */
int crc32c_init(crc32c_context_t *ctx);

/* Compute CRC32c (scalar fallback) */
uint32_t crc32c_scalar(const uint8_t *data, size_t len);

/* Compute CRC32c with SIMD acceleration if available */
uint32_t crc32c_simd(const uint8_t *data, size_t len);

/* Process data incrementally */
void crc32c_update(crc32c_context_t *ctx, const uint8_t *data, size_t len);

/* Finalize CRC32c computation */
uint32_t crc32c_finalize(crc32c_context_t *ctx);

/* ============================================================================
 * SIMD Manifest Validation
 * ============================================================================ */

typedef struct {
  uint8_t name[64];
  uint8_t version[32];
  uint32_t arch_flags;
  uint32_t api_level;
  uint8_t sha256[32];
  uint32_t crc32c;
  uint64_t coherence_phi;
  uint32_t toroidal_depth;
  uint16_t dep_count;
  uint16_t deps[16];
} manifest_entry_simd_t;

/* Validate manifest entry with SIMD-accelerated CRC32c check */
int manifest_validate_simd(const manifest_entry_simd_t *entry);

/* Batch validate multiple manifest entries */
int manifest_validate_batch(const manifest_entry_simd_t *entries, uint32_t count);

/* ============================================================================
 * Benchmark & Profiling
 * ============================================================================ */

typedef struct {
  uint64_t cycles_scalar;
  uint64_t cycles_simd;
  double speedup;
  size_t bytes_processed;
  uint32_t iterations;
} simd_benchmark_t;

/* Run CRC32c benchmark */
int simd_benchmark_crc32c(size_t data_size, uint32_t iterations, simd_benchmark_t *result);

/* Report SIMD capabilities and performance */
void simd_report(FILE *out);

#endif  /* MANIFEST_SIMD_H */
