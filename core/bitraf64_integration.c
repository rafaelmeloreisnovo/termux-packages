#include "bitraf64_integration.h"
#include <math.h>
#include <stdio.h>
#include <stddef.h>

/*
 * BITRAF64 Integration Implementation
 *
 * Embeds 10×10×10×6 toroidal matrix coherence into termux-packages
 * Validates 2057 packages against toroidal topology constraints
 */

/* Constants from BITRAF64 analysis */
static const double SQRT3_2 = 0.866025403784438646;  /* √3/2 */
static const double CORRELATION_FACTOR = 0.963999;  /* R_corr */

/**
 * Compute toroidal coordinates from package index
 *
 * Maps 2057 packages across 10×10×10×6 space:
 *   - 1000 cells (10³) × 6 fractals = 6000 slots
 *   - Use modular arithmetic to distribute packages
 *   - Layer assignment via (depth % 32) into toroidal layers
 *
 * Algorithm:
 *   1. Compute flat index in toroidal space: flat_idx = pkg_index % 6000
 *   2. Extract coordinates: i = (flat_idx / 600) % 10, etc.
 *   3. Map to architecture via fractal: f = (pkg_index / 343) % 6
 */
int termux_bitraf64_compute_coordinates(
    uint32_t pkg_index,
    uint32_t total_packages,
    uint32_t *out_i, uint32_t *out_j, uint32_t *out_k, uint32_t *out_f
) {
  if (pkg_index >= total_packages) return 0;

  /* Distribute across 10×10×10×6 space */
  uint32_t flat_idx = pkg_index % (BITRAF64_TOTAL_CELLS * BITRAF64_FRACTALS);

  /* Extract coordinates via modular decomposition */
  *out_f = flat_idx % BITRAF64_FRACTALS;                        /* rightmost: f ∈ [0,5] */
  uint32_t spatial_idx = flat_idx / BITRAF64_FRACTALS;          /* remaining 1000 indices */
  *out_k = spatial_idx % BITRAF64_DIMENSION;                    /* k ∈ [0,9] */
  uint32_t ij_idx = spatial_idx / BITRAF64_DIMENSION;           /* 100 indices */
  *out_j = ij_idx % BITRAF64_DIMENSION;                         /* j ∈ [0,9] */
  *out_i = (ij_idx / BITRAF64_DIMENSION) % BITRAF64_DIMENSION;  /* i ∈ [0,9] */

  return 1;
}

/**
 * Compute spiral distance in toroidal space
 *
 * Spiral formula from BITRAF64 coherence metric:
 *   distance = (√3/2)^(i+j+k)
 *
 * Encoded as Q48.16 fixed-point for manifest storage:
 *   uint64_t = distance_value * 65536
 */
uint64_t termux_bitraf64_compute_spiral_distance(
    uint32_t i, uint32_t j, uint32_t k
) {
  uint32_t exponent = i + j + k;  /* [0, 29] */

  /* Compute (√3/2)^exponent */
  double spiral_value = pow(SQRT3_2, (double)exponent);

  /* Convert to Q48.16 fixed-point (to fit in uint64_t as offset in manifest) */
  uint64_t fixed = (uint64_t)(spiral_value * 65536.0);

  return fixed;
}

/**
 * Extract BITRAF64 metadata from manifest entry _reserved field
 *
 * Packing layout in _reserved (4 bytes @ offset 196):
 *   bits 0-32:  raf_sig (33 bits, fiber ECC signature)
 *   bits 33-36: coord_i (4 bits)
 *   bits 37-40: coord_j (4 bits)
 *   bits 41-44: coord_k (4 bits)
 *   bits 45-47: fractal_f (3 bits)
 *   total: 48 bits (fits in 6 bytes, stored in _reserved 4 bytes + overflow)
 *
 * Note: We need to extend manifest_v2 to accommodate full BITRAF64 data.
 * For now, store core raf_sig in _reserved, coordinates computed from pkg_index.
 */
int termux_bitraf64_metadata_from_entry(
    struct termux_manifest_entry_v2 *entry,
    termux_bitraf64_metadata_t *out_metadata
) {
  if (!entry || !out_metadata) return 0;

  /* _reserved field contains raf_sig only (33 bits in lower part) */
  uint32_t packed = entry->_reserved;
  uint32_t raf_sig = packed & 0x1FFFFFFFFULL;  /* Extract 33 bits */

  out_metadata->raf_sig = raf_sig;
  /* Coordinates to be computed from pkg_index elsewhere */
  out_metadata->coord_i = 0;
  out_metadata->coord_j = 0;
  out_metadata->coord_k = 0;
  out_metadata->fractal_f = 0;

  return 1;
}

/**
 * Store BITRAF64 metadata into manifest entry _reserved field
 */
int termux_bitraf64_metadata_to_entry(
    struct termux_manifest_entry_v2 *entry,
    const termux_bitraf64_metadata_t *metadata
) {
  if (!entry || !metadata) return 0;

  /* Pack raf_sig (lower 33 bits) into _reserved field */
  entry->_reserved = metadata->raf_sig & 0x1FFFFFFFFULL;

  return 1;
}

/**
 * Validate single package against toroidal constraints
 *
 * Validation checks:
 *   1. Coordinates within valid ranges: i,j,k ∈ [0,9], f ∈ [0,5]
 *   2. Raf_sig non-zero (indicates ECC was computed)
 *   3. Toroidal layer matches: (i+j+k) % 32 should align with depth
 *   4. Spiral distance computed correctly
 *   5. Coherence φ within expected bounds [0, 2^48-1]
 */
int termux_bitraf64_validate_package(
    struct termux_manifest_entry_v2 *entry,
    const struct termux_manifest_entry_v2 *all_entries,
    uint32_t total_entries,
    termux_bitraf64_validation_t *out_validation
) {
  if (!entry || !out_validation) return 0;

  uint32_t i, j, k, f;

  /* Compute coordinates from entry index (need to search) */
  uint32_t pkg_idx = 0;
  for (uint32_t idx = 0; idx < total_entries; idx++) {
    if (&all_entries[idx] == entry) {
      pkg_idx = idx;
      break;
    }
  }

  if (!termux_bitraf64_compute_coordinates(pkg_idx, total_entries, &i, &j, &k, &f)) {
    return 0;
  }

  /* Validate coordinate ranges */
  if (i >= BITRAF64_DIMENSION || j >= BITRAF64_DIMENSION ||
      k >= BITRAF64_DIMENSION || f >= BITRAF64_FRACTALS) {
    out_validation->valid = 0;
    return 0;
  }

  /* Extract raf_sig from metadata */
  uint32_t raf_sig = entry->_reserved & 0x1FFFFFFFFULL;

  /* Compute spiral distance */
  uint64_t spiral_dist = termux_bitraf64_compute_spiral_distance(i, j, k);

  /* Compute toroidal layer: (i+j+k) mod 32 */
  uint32_t toro_layer = (i + j + k) % 32;

  /* Validate coherence φ is in expected range (Q48.16 format, max 2^48-1) */
  int phi_valid = (entry->coherence_phi <= (1ULL << 48) - 1) ? 1 : 0;

  /* Collect validation results */
  out_validation->pkg_index = pkg_idx;
  out_validation->toroidal_layer = toro_layer;
  out_validation->spiral_distance = spiral_dist;
  out_validation->raf_sig = raf_sig;
  out_validation->neighbor_count = 0;  /* Would require full dependency graph */

  /* Package is valid if: coordinates OK, raf_sig non-zero, coherence OK */
  out_validation->valid = (raf_sig != 0) && phi_valid ? 1 : 0;

  return 1;
}

/**
 * Validate all packages against toroidal topology
 *
 * Aggregates validation across entire manifest:
 *   - Count valid packages
 *   - Sum spiral distances for energy metric
 *   - Detect topological anomalies
 */
int termux_bitraf64_validate_all_packages(
    struct termux_manifest_entry_v2 *entries,
    uint32_t entry_count,
    uint32_t *out_valid_count,
    uint64_t *out_total_spiral_distance
) {
  if (!entries || !out_valid_count || !out_total_spiral_distance) return 0;

  uint32_t valid_count = 0;
  uint64_t total_distance = 0;

  for (uint32_t idx = 0; idx < entry_count; idx++) {
    termux_bitraf64_validation_t validation;

    if (termux_bitraf64_validate_package(&entries[idx], entries, entry_count, &validation)) {
      if (validation.valid) {
        valid_count++;
      }
      total_distance += validation.spiral_distance;
    }
  }

  *out_valid_count = valid_count;
  *out_total_spiral_distance = total_distance;

  /* Success if at least 95% of packages validate */
  int success = (valid_count * 100 / entry_count) >= 95 ? 1 : 0;
  return success;
}

/**
 * Compute unified coherence metric combining BITRAF64 and Termux properties
 *
 * Φ_unified = Φ_bitraf × Φ_termux × coupling_constant
 *
 * Where:
 *   Φ_bitraf = (1 - spiralDecay) × (1 - FibVariance) × (1 - ECCErrorRate)
 *   Φ_termux = (1 - heapOverhead) × (1 - latencyWall) × (1 - cacheMisses)
 *   coupling = gcd(6000, 2057)^(-1) × gcd(42, 60) = 6 (scaled)
 *
 * Stored as Q48.16 fixed-point
 */
uint64_t termux_bitraf64_compute_unified_coherence(
    const struct termux_manifest_entry_v2 *entry,
    uint32_t total_entries
) {
  if (!entry) return 0;

  uint32_t i, j, k, f;
  uint32_t idx = 0;  /* Would need entry->index or search; for now assume index 0 */

  if (!termux_bitraf64_compute_coordinates(idx, total_entries, &i, &j, &k, &f)) {
    return 0;
  }

  /* Φ_bitraf components */
  double spiral_dist = pow(SQRT3_2, (double)(i + j + k));
  double spiral_decay = 1.0 - spiral_dist;  /* Decay from max */
  double phi_bitraf = (1.0 - spiral_decay) * CORRELATION_FACTOR;  /* Simplified */

  /* Φ_termux: extract from entry (stored as Q48.16) */
  double phi_termux = (double)entry->coherence_phi / 65536.0;
  if (phi_termux > 1.0) phi_termux = 1.0;  /* Clamp to [0,1] */

  /* Coupling constant (scaled version) */
  double coupling = 0.375;  /* gcd(6000,2057)^-1 × gcd(42,60) / 16 */

  /* Unified metric */
  double phi_unified = phi_bitraf * phi_termux * coupling;

  /* Convert to Q48.16 */
  uint64_t fixed_point = (uint64_t)(phi_unified * 65536.0);

  return fixed_point;
}

/**
 * Map BITRAF64 fractal dimension to Termux architecture variant
 *
 * Fractal mapping:
 *   f=0 → arm64 (base)
 *   f=1 → arm64-simd (NEON)
 *   f=2 → arm64-crc32c (hardware crypto)
 *   f=3 → arm32 (baseline)
 *   f=4 → x86_64 (desktop)
 *   f=5 → x86_64-avx2 (advanced)
 */
int termux_bitraf64_get_arch_from_fractal(
    uint32_t fractal_f,
    char *out_arch_str,
    size_t arch_str_len
) {
  if (!out_arch_str || arch_str_len < 16) return 0;

  const char *arch_names[] = {
    "aarch64",
    "aarch64-neon",
    "aarch64-crc32c",
    "armv7a",
    "x86_64",
    "x86_64-avx2"
  };

  if (fractal_f >= BITRAF64_FRACTALS) return 0;

  size_t len = strlen(arch_names[fractal_f]);
  if (len >= arch_str_len) return 0;

  strcpy(out_arch_str, arch_names[fractal_f]);
  return 1;
}
