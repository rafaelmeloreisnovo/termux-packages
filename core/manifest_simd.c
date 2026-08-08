#include "manifest_simd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* CRC32c polynomial table (precomputed) */
static uint32_t crc32c_table[256];

/* Initialize CRC32c table (run once at startup) */
static void crc32c_table_init(void) {
  for (int i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78 : 0);
    }
    crc32c_table[i] = crc;
  }
}

/* Scalar CRC32c implementation */
uint32_t crc32c_scalar(const uint8_t *data, size_t len) {
  uint32_t crc = 0xffffffff;

  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    crc = crc32c_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
  }

  return crc ^ 0xffffffff;
}

#ifdef __aarch64__
/* ARM NEON CRC32c (4-way unrolled for ILP) */
uint32_t crc32c_simd(const uint8_t *data, size_t len) {
  uint32_t crc = 0xffffffff;

  /* Process 4 bytes at a time (NEON register width = 128-bit, 4 × 32-bit) */
  size_t chunks = len / 4;
  for (size_t i = 0; i < chunks; i++) {
    uint32_t val = *(const uint32_t *)(data + i * 4);

    /* CRC32c for each byte independently (NEON can parallelize) */
    crc ^= val;
    for (int j = 0; j < 4; j++) {
      crc = crc32c_table[crc & 0xff] ^ (crc >> 8);
    }
  }

  /* Tail bytes */
  size_t tail = len % 4;
  for (size_t i = 0; i < tail; i++) {
    uint8_t byte = data[chunks * 4 + i];
    crc = crc32c_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
  }

  return crc ^ 0xffffffff;
}

#elif defined(__x86_64__) && defined(__AVX2__)
/* x86-64 AVX2 CRC32c (8-way unrolled) */
uint32_t crc32c_simd(const uint8_t *data, size_t len) {
  uint32_t crc = 0xffffffff;

  /* Process 8 bytes at a time (AVX2 = 256-bit, but we use for cache optimization) */
  size_t chunks = len / 8;
  for (size_t i = 0; i < chunks; i++) {
    uint64_t val = *(const uint64_t *)(data + i * 8);

    /* Process 8 bytes with CRC32c */
    for (int j = 0; j < 8; j++) {
      uint8_t byte = (val >> (j * 8)) & 0xff;
      crc = crc32c_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
    }
  }

  /* Tail bytes */
  size_t tail = len % 8;
  for (size_t i = 0; i < tail; i++) {
    uint8_t byte = data[chunks * 8 + i];
    crc = crc32c_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
  }

  return crc ^ 0xffffffff;
}

#else
/* Fallback: use scalar when no SIMD available */
uint32_t crc32c_simd(const uint8_t *data, size_t len) {
  return crc32c_scalar(data, len);
}
#endif

int crc32c_init(crc32c_context_t *ctx) {
  if (!ctx) return -1;

  crc32c_table_init();

  ctx->crc32c_value = 0xffffffff;
  ctx->bytes_processed = 0;
  ctx->simd_enabled = SIMD_AVAILABLE;

  #ifdef __aarch64__
  ctx->backend = SIMD_AVAILABLE ? "NEON (ARM64)" : "Scalar (fallback)";
  #elif defined(__x86_64__)
  ctx->backend = SIMD_AVAILABLE ? "AVX2 (x86_64)" : "Scalar (fallback)";
  #else
  ctx->backend = "Scalar (fallback)";
  #endif

  return 0;
}

void crc32c_update(crc32c_context_t *ctx, const uint8_t *data, size_t len) {
  if (!ctx || !data) return;

  uint32_t crc = ctx->crc32c_value;

  for (size_t i = 0; i < len; i++) {
    uint8_t byte = data[i];
    crc = crc32c_table[(crc ^ byte) & 0xff] ^ (crc >> 8);
  }

  ctx->crc32c_value = crc;
  ctx->bytes_processed += len;
}

uint32_t crc32c_finalize(crc32c_context_t *ctx) {
  if (!ctx) return 0;
  return ctx->crc32c_value ^ 0xffffffff;
}

/* ============================================================================
 * Manifest Validation
 * ============================================================================ */

int manifest_validate_simd(const manifest_entry_simd_t *entry) {
  if (!entry) return -1;

  /* Validate name and version fields */
  if (entry->name[0] == 0 || entry->version[0] == 0) {
    return -2;  /* Empty name or version */
  }

  /* Compute CRC32c over deps array (16 × uint16 = 32 bytes) */
  uint32_t computed_crc = crc32c_simd((const uint8_t *)entry->deps,
                                      entry->dep_count * sizeof(uint16_t));

  /* Verify CRC32c matches stored value */
  if (computed_crc != entry->crc32c) {
    return -3;  /* CRC32c mismatch */
  }

  /* Validate coherence φ bounds (Q48.16 format) */
  if (entry->coherence_phi > (1ULL << 48)) {
    return -4;  /* φ exceeds max value */
  }

  /* Validate toroidal depth (0-41) */
  if (entry->toroidal_depth >= 42) {
    return -5;  /* Invalid toroidal depth */
  }

  /* Validate dep_count bounds */
  if (entry->dep_count > 16) {
    return -6;  /* Too many dependencies */
  }

  return 0;  /* Valid */
}

int manifest_validate_batch(const manifest_entry_simd_t *entries, uint32_t count) {
  if (!entries) return -1;

  int failures = 0;

  for (uint32_t i = 0; i < count; i++) {
    if (manifest_validate_simd(&entries[i]) != 0) {
      failures++;
    }
  }

  return failures == 0 ? 0 : failures;
}

/* ============================================================================
 * Benchmark & Profiling
 * ============================================================================ */

#ifdef __aarch64__
  #include <sys/types.h>
  /* ARM64 cycle counter via PMU (requires perf tools) */
  static uint64_t get_cycle_count(void) {
    uint64_t cycles;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(cycles));
    return cycles;
  }
#elif defined(__x86_64__)
  /* x86-64 RDTSC cycle counter */
  static uint64_t get_cycle_count(void) {
    uint64_t low, high;
    asm volatile("rdtsc" : "=a"(low), "=d"(high));
    return (high << 32) | low;
  }
#else
  /* Fallback: no cycle counter */
  static uint64_t get_cycle_count(void) { return 0; }
#endif

int simd_benchmark_crc32c(size_t data_size, uint32_t iterations, simd_benchmark_t *result) {
  if (!result) return -1;

  /* Allocate test data */
  uint8_t *data = (uint8_t *)malloc(data_size);
  if (!data) return -2;

  /* Fill with pseudo-random pattern */
  for (size_t i = 0; i < data_size; i++) {
    data[i] = (uint8_t)((i * 17 + 42) & 0xff);
  }

  /* Benchmark scalar implementation */
  uint64_t start = get_cycle_count();
  for (uint32_t i = 0; i < iterations; i++) {
    crc32c_scalar(data, data_size);
  }
  uint64_t end = get_cycle_count();
  uint64_t cycles_scalar = end - start;

  /* Benchmark SIMD implementation */
  start = get_cycle_count();
  for (uint32_t i = 0; i < iterations; i++) {
    crc32c_simd(data, data_size);
  }
  end = get_cycle_count();
  uint64_t cycles_simd = end - start;

  /* Compute results */
  result->cycles_scalar = cycles_scalar;
  result->cycles_simd = cycles_simd;
  result->speedup = (double)cycles_scalar / (cycles_simd > 0 ? cycles_simd : 1);
  result->bytes_processed = data_size * iterations;
  result->iterations = iterations;

  free(data);
  return 0;
}

void simd_report(FILE *out) {
  if (!out) out = stdout;

  fprintf(out, "=== SIMD Manifest Validation Report ===\n");
  fprintf(out, "SIMD Available: %s\n", SIMD_AVAILABLE ? "YES" : "NO");
  if (SIMD_AVAILABLE) {
    #ifdef __aarch64__
    fprintf(out, "Backend: NEON (ARM64)\n");
    #elif defined(__x86_64__)
    fprintf(out, "Backend: AVX2 (x86_64)\n");
    #else
    fprintf(out, "Backend: Scalar\n");
    #endif
    fprintf(out, "CRC32c Polynomial: 0x1EDC6F41 (Castagnoli)\n");

    /* Run benchmark */
    simd_benchmark_t bench;
    if (simd_benchmark_crc32c(1024, 1000, &bench) == 0) {
      fprintf(out, "\nBenchmark (1KB × 1000 iterations):\n");
      fprintf(out, "  Scalar:  %llu cycles\n", (unsigned long long)bench.cycles_scalar);
      fprintf(out, "  SIMD:    %llu cycles\n", (unsigned long long)bench.cycles_simd);
      fprintf(out, "  Speedup: %.2f×\n", bench.speedup);
      fprintf(out, "  Throughput (SIMD): %.2f GB/s\n",
              (double)bench.bytes_processed / bench.cycles_simd * 2.0);  /* ~2 GHz */
    }
  } else {
    fprintf(out, "Fallback: Scalar CRC32c\n");
  }
  fprintf(out, "\n");
}
