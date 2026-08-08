#ifndef TERMUX_BITRAF64_INTEGRATION_H
#define TERMUX_BITRAF64_INTEGRATION_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "manifest_v2.h"

/*
 * BITRAF64 Integration Layer for Termux Manifest V2
 * Maps BITRAF64 toroidal matrix (10×10×10×6) to 2057 packages
 * Embeds fiber ECC (33-bit raf_sig) into manifest entries
 * Validates toroidal ordering and coherence properties
 */

#define BITRAF64_DIMENSION 10           /* 10×10×10 spatial */
#define BITRAF64_FRACTALS 6             /* 6 fractal variants */
#define BITRAF64_TOTAL_CELLS 1000       /* 10³ = 1000 */
#define BITRAF64_ECC_RAF_SIG_BITS 33    /* Fiber ECC syndrome */
#define BITRAF64_MAX_PACKAGES 2057      /* Termux packages */

/* Bit-packing for BITRAF64 metadata in manifest reserved field */
typedef struct {
  uint64_t raf_sig : 33;      /* Fiber ECC signature (33 bits) */
  uint64_t coord_i : 4;       /* Toroidal coordinate i ∈ [0,9] (4 bits) */
  uint64_t coord_j : 4;       /* Toroidal coordinate j ∈ [0,9] (4 bits) */
  uint64_t coord_k : 4;       /* Toroidal coordinate k ∈ [0,9] (4 bits) */
  uint64_t fractal_f : 3;     /* Fractal index f ∈ [0,5] (3 bits) */
} termux_bitraf64_metadata_t;

/* Toroidal validation result */
typedef struct {
  uint32_t pkg_index;         /* Package index in manifest */
  uint32_t toroidal_layer;    /* Computed layer from (i+j+k) % 32 */
  uint64_t spiral_distance;   /* Distance metric in toroidal space */
  uint32_t neighbor_count;    /* Number of valid neighbors */
  uint32_t raf_sig;           /* Fiber ECC signature value */
  int valid;                  /* 1 if passes all validations */
} termux_bitraf64_validation_t;

/* Adapter functions */

/**
 * Compute toroidal coordinates from package index
 * Maps 2057 packages to 10×10×10×6 matrix
 *
 * Strategy:
 *   - Sort packages by dependency depth (existing)
 *   - Assign to toroidal layers: depth % 32
 *   - Within layer: distribute across (i,j,k,f) space
 *   - Fractal f cycles through 6 variants per package
 */
int termux_bitraf64_compute_coordinates(
    uint32_t pkg_index,
    uint32_t total_packages,
    uint32_t *out_i, uint32_t *out_j, uint32_t *out_k, uint32_t *out_f
);

/**
 * Compute spiral distance in toroidal space
 * Distance = (√3/2)^(i+j+k) as per BITRAF64 coherence metric
 */
uint64_t termux_bitraf64_compute_spiral_distance(
    uint32_t i, uint32_t j, uint32_t k
);

/**
 * Extract or set BITRAF64 metadata in manifest entry
 * Uses _reserved field for bit-packed storage
 */
int termux_bitraf64_metadata_from_entry(
    struct termux_manifest_entry_v2 *entry,
    termux_bitraf64_metadata_t *out_metadata
);

int termux_bitraf64_metadata_to_entry(
    struct termux_manifest_entry_v2 *entry,
    const termux_bitraf64_metadata_t *metadata
);

/**
 * Validate package against toroidal constraints
 *
 * Checks:
 *   1. Coordinates (i,j,k,f) within valid ranges
 *   2. Raf_sig consistent with coordinate hash
 *   3. Toroidal layer matches expected DAG depth
 *   4. Neighbors are reachable in dependency graph
 */
int termux_bitraf64_validate_package(
    struct termux_manifest_entry_v2 *entry,
    const struct termux_manifest_entry_v2 *all_entries,
    uint32_t total_entries,
    termux_bitraf64_validation_t *out_validation
);

/**
 * Validate entire manifest against toroidal topology
 * Ensures 2057 packages form coherent toroidal lattice
 */
int termux_bitraf64_validate_all_packages(
    struct termux_manifest_entry_v2 *entries,
    uint32_t entry_count,
    uint32_t *out_valid_count,
    uint64_t *out_total_spiral_distance
);

/**
 * Compute unified coherence metric
 * Φ_unified = Φ_bitraf × Φ_termux × coupling_constant
 *
 * Where:
 *   Φ_bitraf = (1 - spiralDecay) × (1 - FibonacciVariance) × (1 - ECCErrorRate)
 *   Φ_termux = (1 - heapOverhead) × (1 - latencyWall) × (1 - cacheMisses)
 */
uint64_t termux_bitraf64_compute_unified_coherence(
    const struct termux_manifest_entry_v2 *entry,
    uint32_t total_entries
);

/**
 * Map BITRAF64 layout to Termux architecture variants
 * BITRAF64 fractals (6) → Termux architectures (ARM32, ARM64, x86_64, with variants)
 */
int termux_bitraf64_get_arch_from_fractal(
    uint32_t fractal_f,
    char *out_arch_str,
    size_t arch_str_len
);

/**
 * Pack/unpack operations for bit-level BITRAF64 metadata
 */
static inline uint64_t termux_bitraf64_metadata_pack(
    uint32_t raf_sig,      /* 33 bits, masked to lower 33 */
    uint32_t coord_i, uint32_t coord_j, uint32_t coord_k,
    uint32_t fractal_f
) {
  return ((uint64_t)(raf_sig & 0x1FFFFFFFFULL) << 0) |   /* bits 0-32: raf_sig */
         ((uint64_t)(coord_i & 0xF) << 33) |              /* bits 33-36: i */
         ((uint64_t)(coord_j & 0xF) << 37) |              /* bits 37-40: j */
         ((uint64_t)(coord_k & 0xF) << 41) |              /* bits 41-44: k */
         ((uint64_t)(fractal_f & 0x7) << 45);             /* bits 45-47: f */
}

static inline void termux_bitraf64_metadata_unpack(
    uint64_t packed,
    uint32_t *out_raf_sig,
    uint32_t *out_i, uint32_t *out_j, uint32_t *out_k,
    uint32_t *out_f
) {
  *out_raf_sig = (uint32_t)((packed >> 0) & 0x1FFFFFFFFULL);
  *out_i = (uint32_t)((packed >> 33) & 0xF);
  *out_j = (uint32_t)((packed >> 37) & 0xF);
  *out_k = (uint32_t)((packed >> 41) & 0xF);
  *out_f = (uint32_t)((packed >> 45) & 0x7);
}

#endif /* TERMUX_BITRAF64_INTEGRATION_H */
