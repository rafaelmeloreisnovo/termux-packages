#ifndef TERMUX_BITRAF64_SIMD_OPS_H
#define TERMUX_BITRAF64_SIMD_OPS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * BITRAF64 SIMD Optimization Layer (Phase 2)
 *
 * Vectorized operations for:
 *   1. Spiral distance calculation: (√3/2)^(i+j+k)
 *   2. Parity computations (XOR reductions)
 *   3. Fiber ECC matrix operations
 *   4. Coherence aggregation (mean φ over packages)
 *
 * Supports multiple backends: NEON (ARM), SVE, AVX2/512 (x86)
 * Automatic fallback to scalar when SIMD unavailable
 */

/* SIMD Backend Detection */
typedef enum {
  BITRAF64_SIMD_SCALAR = 0,     /* No SIMD (always available) */
  BITRAF64_SIMD_NEON = 1,       /* ARM NEON 128-bit */
  BITRAF64_SIMD_SVE = 2,        /* ARM SVE 256/512-bit */
  BITRAF64_SIMD_AVX2 = 3,       /* x86 AVX2 256-bit */
  BITRAF64_SIMD_AVX512 = 4      /* x86 AVX-512 512-bit */
} bitraf64_simd_backend_t;

/* SIMD Capability Flags */
typedef struct {
  bitraf64_simd_backend_t backend;
  uint32_t vector_width;         /* Bytes: 0 (scalar), 16 (NEON), 32 (AVX2), 64 (AVX512) */
  int has_crypto;                /* CRC32c hardware support */
  int has_fma;                   /* Fused multiply-add */
  int has_reduction;             /* Native reduction operations */
} bitraf64_simd_caps_t;

/* Detect available SIMD backend */
bitraf64_simd_backend_t bitraf64_simd_detect_backend(void);

/* Initialize SIMD capabilities structure */
int bitraf64_simd_init(bitraf64_simd_caps_t *caps);

/*
 * Vectorized spiral distance computation
 *
 * Computes (√3/2)^exponent for multiple exponents in parallel
 *
 * Input:  exponents[n] = (i+j+k) for n packages
 * Output: distances[n] = (√3/2)^exponents[n] as Q48.16 fixed-point
 *
 * Speedup: 4-8× depending on backend (4 parallel lanes minimum)
 */
int bitraf64_simd_spiral_distances(
    const uint32_t *exponents,
    size_t count,
    uint64_t *out_distances,
    const bitraf64_simd_caps_t *caps
);

/*
 * Vectorized XOR parity reduction
 *
 * Reduces array of bytes via XOR to single bit
 * Used for lane_parity and block_parity computation
 *
 * Input:  data[n] = byte stream
 * Output: single bit (0 or 1)
 *
 * Speedup: 4-8× (process 4-8 bytes per cycle)
 */
uint8_t bitraf64_simd_parity_xor(
    const uint8_t *data,
    size_t len,
    const bitraf64_simd_caps_t *caps
);

/*
 * Vectorized fiber ECC parity matrix multiplication
 *
 * Computes: p = H × data (mod 2) for multiple parities in parallel
 * H is [10×16] or [10×32] parity check matrix
 *
 * Input:  data[block_size] = input block
 *         H_matrix[10][matrix_width] = parity matrix
 * Output: parity_bits[10] = syndrome result
 *
 * Speedup: 3-4× (vectorize XOR chains)
 */
int bitraf64_simd_parity_matrix_mult(
    const uint8_t *data,
    size_t data_len,
    const uint8_t *H_matrix,
    uint32_t matrix_rows,
    uint32_t matrix_cols,
    uint16_t *out_parity_bits,
    const bitraf64_simd_caps_t *caps
);

/*
 * Vectorized coherence aggregation (mean φ over packages)
 *
 * Computes: mean_phi = (Σ phi_i) / count
 * Using SIMD reduction for parallel summation
 *
 * Input:  phi_scores[count] = individual φ values (Q48.16)
 *         count = number of packages
 * Output: mean_phi = average coherence (Q48.16)
 *
 * Speedup: 3-4× (vectorize reduction across lanes)
 */
uint64_t bitraf64_simd_coherence_mean(
    const uint64_t *phi_scores,
    size_t count,
    const bitraf64_simd_caps_t *caps
);

/*
 * Vectorized CRC32c computation (4-unroll for throughput)
 *
 * Processes 4 independent 64-byte blocks in parallel
 * Each block gets its own CRC32c accumulator (no data dependency)
 *
 * Input:  blocks[4][64] = 4 independent blocks
 * Output: crcs[4] = 4 CRC32c checksums
 *
 * Speedup: 4× (process 4 blocks concurrently)
 */
int bitraf64_simd_crc32c_quad(
    const uint8_t *block0,
    const uint8_t *block1,
    const uint8_t *block2,
    const uint8_t *block3,
    uint32_t *out_crc0,
    uint32_t *out_crc1,
    uint32_t *out_crc2,
    uint32_t *out_crc3,
    const bitraf64_simd_caps_t *caps
);

/*
 * Query SIMD operation performance
 * Returns estimated speedup vs scalar for given operation
 */
float bitraf64_simd_get_speedup(
    const bitraf64_simd_caps_t *caps,
    const char *operation_name
);

#endif /* TERMUX_BITRAF64_SIMD_OPS_H */
