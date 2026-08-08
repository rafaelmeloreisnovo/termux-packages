#ifndef TERMUX_BITRAF64_INTEGRATION_H
#define TERMUX_BITRAF64_INTEGRATION_H

#include <stddef.h>
#include <stdint.h>

#include "manifest_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BITRAF64_DIM 10U
#define BITRAF64_FRACTALS 6U
#define BITRAF64_CELLS_PER_FRACTAL 1000U
#define BITRAF64_TOTAL_CELLS 6000U
#define BITRAF64_PACKAGE_REFERENCE_COUNT 2057U
#define BITRAF64_PROJECTED_LAYERS 32U

#define BITRAF64_PHI 1.6180339887498948482
#define BITRAF64_PI 3.14159265358979323846
#define BITRAF64_SPIRAL 0.86602540378443864676
#define BITRAF64_R_CORR_DECLARED 0.963999

typedef struct {
    uint8_t i;
    uint8_t j;
    uint8_t k;
    uint8_t f;
} bitraf64_coord_t;

/* Manifest V2 remains exactly 200 bytes. Full Phase-1 diagnostics live here. */
typedef struct {
    uint64_t diagnostic_sig33;
    uint8_t projected_layer;
    uint8_t coherence_score7;
    uint16_t flags;
    uint32_t manifest_tag32;
} bitraf64_sidecar_v1_t;

enum {
    BITRAF64_SIDECAR_STRUCTURAL_ONLY = 1u << 0,
    BITRAF64_SIDECAR_NOT_ECC_PROOF   = 1u << 1,
    BITRAF64_SIDECAR_NOT_DAG_DEPTH   = 1u << 2
};

typedef struct {
    uint8_t torus_roundtrip_verified;
    uint8_t manifest_tag_roundtrip_verified;
    uint8_t gcd_claim_corrected;
    uint8_t r_corr_formula_matches_declared;
    uint8_t linear_invertibility_proven;
    uint8_t single_error_correction_proven;
    uint8_t real_dag_alignment_proven;
    uint8_t entropy_autocorrelation_measured;
    uint8_t simd_speedup_benchmarked;
    uint8_t claim_allowed;
} bitraf64_claim_gate_t;

int bitraf64_coord_from_index(uint32_t index, bitraf64_coord_t *out);
uint32_t bitraf64_coord_to_index(const bitraf64_coord_t *coord);
uint8_t bitraf64_projected_layer(const bitraf64_coord_t *coord);
double bitraf64_toroidal_distance(const bitraf64_coord_t *a,
                                  const bitraf64_coord_t *b);

/* Diagnostic fingerprint only: this is not an ECC correction proof. */
uint64_t bitraf64_diagnostic_sig33(const uint8_t *data, size_t len);

uint32_t bitraf64_gcd_u32(uint32_t a, uint32_t b);
double bitraf64_r_corr_derived(void);
double bitraf64_redundancy_bits_per_byte(uint32_t redundancy_bits,
                                         uint32_t input_bytes);

uint8_t bitraf64_coherence_score7(const struct termux_manifest_entry_v2 *entry,
                                  const uint8_t *data,
                                  size_t len);

uint32_t bitraf64_manifest_tag32(const struct termux_manifest_entry_v2 *entry,
                                 const uint8_t *data,
                                 size_t len,
                                 uint8_t projected_layer);

int bitraf64_manifest_embed(struct termux_manifest_entry_v2 *entry,
                            bitraf64_sidecar_v1_t *sidecar,
                            const uint8_t *data,
                            size_t len,
                            uint32_t package_index);

int bitraf64_manifest_validate(const struct termux_manifest_entry_v2 *entry,
                               const bitraf64_sidecar_v1_t *sidecar,
                               const uint8_t *data,
                               size_t len,
                               uint32_t package_index);

bitraf64_claim_gate_t
bitraf64_claim_gate_collect(const struct termux_manifest_entry_v2 *entry,
                            const bitraf64_sidecar_v1_t *sidecar,
                            const uint8_t *data,
                            size_t len,
                            uint32_t package_index);

#ifdef __cplusplus
}
#endif

#endif
