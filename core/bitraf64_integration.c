#include "bitraf64_integration.h"

#include <math.h>

static uint32_t bitraf64_mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= UINT32_C(0x7feb352d);
    x ^= x >> 15;
    x *= UINT32_C(0x846ca68b);
    x ^= x >> 16;
    return x;
}

int bitraf64_coord_from_index(uint32_t index, bitraf64_coord_t *out)
{
    uint32_t rem;
    if (out == NULL || index >= BITRAF64_TOTAL_CELLS) return 0;
    out->f = (uint8_t)(index / BITRAF64_CELLS_PER_FRACTAL);
    rem = index % BITRAF64_CELLS_PER_FRACTAL;
    out->i = (uint8_t)(rem / 100U);
    rem %= 100U;
    out->j = (uint8_t)(rem / 10U);
    out->k = (uint8_t)(rem % 10U);
    return 1;
}

uint32_t bitraf64_coord_to_index(const bitraf64_coord_t *coord)
{
    if (coord == NULL || coord->i >= BITRAF64_DIM || coord->j >= BITRAF64_DIM ||
        coord->k >= BITRAF64_DIM || coord->f >= BITRAF64_FRACTALS) return UINT32_MAX;
    return (uint32_t)coord->f * BITRAF64_CELLS_PER_FRACTAL +
           (uint32_t)coord->i * 100U + (uint32_t)coord->j * 10U + (uint32_t)coord->k;
}

uint8_t bitraf64_projected_layer(const bitraf64_coord_t *coord)
{
    uint32_t spatial;
    if (coord == NULL || coord->i >= BITRAF64_DIM || coord->j >= BITRAF64_DIM ||
        coord->k >= BITRAF64_DIM) return UINT8_MAX;
    spatial = (uint32_t)coord->i * 100U + (uint32_t)coord->j * 10U + (uint32_t)coord->k;
    return (uint8_t)(spatial % BITRAF64_PROJECTED_LAYERS);
}

static unsigned bitraf64_wrap_delta(unsigned a, unsigned b)
{
    unsigned d = a > b ? a - b : b - a;
    unsigned wrap = BITRAF64_DIM - d;
    return d < wrap ? d : wrap;
}

double bitraf64_toroidal_distance(const bitraf64_coord_t *a, const bitraf64_coord_t *b)
{
    unsigned di, dj, dk, exponent;
    double euclidean;
    if (a == NULL || b == NULL || a->i >= BITRAF64_DIM || a->j >= BITRAF64_DIM ||
        a->k >= BITRAF64_DIM || b->i >= BITRAF64_DIM || b->j >= BITRAF64_DIM ||
        b->k >= BITRAF64_DIM) return NAN;
    di = bitraf64_wrap_delta(a->i, b->i);
    dj = bitraf64_wrap_delta(a->j, b->j);
    dk = bitraf64_wrap_delta(a->k, b->k);
    euclidean = sqrt((double)(di * di + dj * dj + dk * dk));
    /* Deterministic weighting retained; not promoted to metric/fractal proof. */
    exponent = (unsigned)a->i + (unsigned)a->j + (unsigned)a->k;
    return euclidean * pow(BITRAF64_SPIRAL, (double)exponent);
}

uint64_t bitraf64_diagnostic_sig33(const uint8_t *data, size_t len)
{
    uint64_t lane_bits = 0, block_bits = 0;
    uint32_t syndrome = UINT32_C(0x1EDC6F41);
    uint8_t lane[10] = {0};
    size_t n, block_size;
    if (data == NULL && len != 0U) return 0;
    for (n = 0; n < len; ++n) {
        lane[n % 10U] ^= data[n];
        syndrome ^= (uint32_t)data[n] << 24;
        syndrome = (syndrome << 1) ^ ((syndrome & UINT32_C(0x80000000)) ? UINT32_C(0x1EDC6F41) : 0U);
    }
    for (n = 0; n < 10U; ++n) if (lane[n] != 0U) lane_bits |= UINT64_C(1) << n;
    block_size = len == 0U ? 1U : (len + 15U) / 16U;
    for (n = 0; n < 16U; ++n) {
        size_t start = n * block_size, end = start + block_size, p;
        uint8_t parity = 0;
        if (start >= len) break;
        if (end > len) end = len;
        for (p = start; p < end; ++p) parity ^= data[p];
        if (parity != 0U) block_bits |= UINT64_C(1) << n;
    }
    return (lane_bits | (block_bits << 10) |
            ((uint64_t)((syndrome >> 25) & 0x7FU) << 26)) & UINT64_C(0x1FFFFFFFF);
}

uint32_t bitraf64_gcd_u32(uint32_t a, uint32_t b)
{
    while (b != 0U) { uint32_t t = a % b; a = b; b = t; }
    return a;
}

double bitraf64_r_corr_derived(void)
{
    return (BITRAF64_PHI * 15.0) / (BITRAF64_PI * 42.0);
}

double bitraf64_redundancy_bits_per_byte(uint32_t redundancy_bits, uint32_t input_bytes)
{
    if (input_bytes == 0U) return NAN;
    return (double)redundancy_bits / (double)input_bytes;
}

uint8_t bitraf64_coherence_score7(const struct termux_manifest_entry_v2 *entry,
                                  const uint8_t *data, size_t len)
{
    uint64_t sig;
    unsigned score = 0;
    if (entry == NULL || (data == NULL && len != 0U)) return 0;
    sig = bitraf64_diagnostic_sig33(data, len);
    if (entry->dep_count <= TERMUX_MANIFEST_MAX_DEPS) score += 24U;
    if (entry->crc32c != 0U) score += 20U;
    if (entry->coherence_phi != 0U) score += 20U;
    if ((sig & UINT64_C(0x3FF)) != 0U) score += 18U;
    if (len != 0U) score += 18U;
    if (score > 127U) score = 127U;
    return (uint8_t)score;
}

uint32_t bitraf64_manifest_tag32(const struct termux_manifest_entry_v2 *entry,
                                 const uint8_t *data, size_t len, uint8_t projected_layer)
{
    uint64_t sig = bitraf64_diagnostic_sig33(data, len);
    uint32_t x = (uint32_t)sig ^ (uint32_t)(sig >> 32);
    x ^= (uint32_t)projected_layer * UINT32_C(0x9e3779b1);
    x ^= (uint32_t)bitraf64_coherence_score7(entry, data, len) << 24;
    x ^= (uint32_t)len * UINT32_C(0x85ebca6b);
    return bitraf64_mix32(x);
}

int bitraf64_manifest_embed(struct termux_manifest_entry_v2 *entry,
                            bitraf64_sidecar_v1_t *sidecar,
                            const uint8_t *data, size_t len, uint32_t package_index)
{
    bitraf64_coord_t coord;
    uint8_t layer;
    uint32_t tag;
    if (entry == NULL || sidecar == NULL || (data == NULL && len != 0U) ||
        package_index >= BITRAF64_PACKAGE_REFERENCE_COUNT ||
        !bitraf64_coord_from_index(package_index, &coord)) return 0;
    layer = bitraf64_projected_layer(&coord);
    tag = bitraf64_manifest_tag32(entry, data, len, layer);
    sidecar->diagnostic_sig33 = bitraf64_diagnostic_sig33(data, len);
    sidecar->projected_layer = layer;
    sidecar->coherence_score7 = bitraf64_coherence_score7(entry, data, len);
    sidecar->flags = BITRAF64_SIDECAR_STRUCTURAL_ONLY |
                     BITRAF64_SIDECAR_NOT_ECC_PROOF |
                     BITRAF64_SIDECAR_NOT_DAG_DEPTH;
    sidecar->manifest_tag32 = tag;
    /* _reserved is 32 bits: persist only a recomputable tag, never silent truncation. */
    entry->_reserved = tag;
    return 1;
}

int bitraf64_manifest_validate(const struct termux_manifest_entry_v2 *entry,
                               const bitraf64_sidecar_v1_t *sidecar,
                               const uint8_t *data, size_t len, uint32_t package_index)
{
    bitraf64_coord_t coord;
    uint8_t layer;
    uint32_t expected;
    if (entry == NULL || sidecar == NULL || (data == NULL && len != 0U) ||
        package_index >= BITRAF64_PACKAGE_REFERENCE_COUNT ||
        !bitraf64_coord_from_index(package_index, &coord)) return 0;
    layer = bitraf64_projected_layer(&coord);
    expected = bitraf64_manifest_tag32(entry, data, len, layer);
    return sidecar->diagnostic_sig33 == bitraf64_diagnostic_sig33(data, len) &&
           sidecar->projected_layer == layer &&
           sidecar->coherence_score7 == bitraf64_coherence_score7(entry, data, len) &&
           sidecar->manifest_tag32 == expected && entry->_reserved == expected;
}

bitraf64_claim_gate_t bitraf64_claim_gate_collect(
    const struct termux_manifest_entry_v2 *entry,
    const bitraf64_sidecar_v1_t *sidecar,
    const uint8_t *data, size_t len, uint32_t package_index)
{
    bitraf64_claim_gate_t gate = {0};
    bitraf64_coord_t coord;
    uint32_t roundtrip = UINT32_MAX;
    double derived = bitraf64_r_corr_derived();
    if (package_index < BITRAF64_TOTAL_CELLS && bitraf64_coord_from_index(package_index, &coord))
        roundtrip = bitraf64_coord_to_index(&coord);
    gate.torus_roundtrip_verified = (roundtrip == package_index);
    gate.manifest_tag_roundtrip_verified =
        bitraf64_manifest_validate(entry, sidecar, data, len, package_index);
    gate.gcd_claim_corrected =
        bitraf64_gcd_u32(BITRAF64_TOTAL_CELLS, BITRAF64_PACKAGE_REFERENCE_COUNT) == 1U;
    gate.r_corr_formula_matches_declared = fabs(derived - BITRAF64_R_CORR_DECLARED) < 1e-12;
    /* Fail closed: Phase 1 does not contain these proof obligations. */
    gate.linear_invertibility_proven = 0;
    gate.single_error_correction_proven = 0;
    gate.real_dag_alignment_proven = 0;
    gate.entropy_autocorrelation_measured = 0;
    gate.simd_speedup_benchmarked = 0;
    gate.claim_allowed = gate.torus_roundtrip_verified &&
        gate.manifest_tag_roundtrip_verified && gate.gcd_claim_corrected &&
        gate.r_corr_formula_matches_declared && gate.linear_invertibility_proven &&
        gate.single_error_correction_proven && gate.real_dag_alignment_proven &&
        gate.entropy_autocorrelation_measured && gate.simd_speedup_benchmarked;
    return gate;
}
