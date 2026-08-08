#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../bitraf64_integration.h"

static void test_all_coordinates_roundtrip(void)
{
    uint32_t n;
    for (n = 0; n < BITRAF64_TOTAL_CELLS; ++n) {
        bitraf64_coord_t c;
        assert(bitraf64_coord_from_index(n, &c));
        assert(bitraf64_coord_to_index(&c) == n);
    }
}

static void test_torus_wraparound(void)
{
    bitraf64_coord_t a = {0, 0, 0, 0};
    bitraf64_coord_t b = {9, 0, 0, 0};
    double d_ab = bitraf64_toroidal_distance(&a, &b);
    double d_ba = bitraf64_toroidal_distance(&b, &a);
    assert(isfinite(d_ab));
    assert(isfinite(d_ba));
    /* Directional Phase-1 weighting means this must not be called a metric. */
    assert(d_ab != d_ba);
    assert(fabs(d_ab - 1.0) < 1e-12);
}

static void test_corrected_arithmetic(void)
{
    double r = bitraf64_r_corr_derived();
    double redundancy = bitraf64_redundancy_bits_per_byte(53U, 1024U);
    assert(bitraf64_gcd_u32(6000U, 2057U) == 1U);
    assert(bitraf64_gcd_u32(42U, 60U) == 6U);
    assert(r > 0.1839 && r < 0.1840);
    assert(fabs(r - BITRAF64_R_CORR_DECLARED) > 0.7);
    assert(fabs(redundancy - 0.0517578125) < 1e-15);
}

static void test_manifest_sidecar_no_silent_truncation(void)
{
    struct termux_manifest_entry_v2 entry;
    bitraf64_sidecar_v1_t sidecar;
    const uint8_t data[] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    memset(&entry, 0, sizeof(entry));
    memset(&sidecar, 0, sizeof(sidecar));
    entry.dep_count = 2;
    entry.crc32c = 0x12345678U;
    entry.coherence_phi = 1U;
    assert(sizeof(entry._reserved) == 4U);
    assert(sizeof(entry) == 200U);
    assert(bitraf64_manifest_embed(&entry, &sidecar, data, sizeof(data), 42U));
    assert(sidecar.diagnostic_sig33 <= UINT64_C(0x1FFFFFFFF));
    assert(sidecar.manifest_tag32 == entry._reserved);
    assert(bitraf64_manifest_validate(&entry, &sidecar, data, sizeof(data), 42U));
    assert((sidecar.flags & BITRAF64_SIDECAR_NOT_ECC_PROOF) != 0U);
    assert((sidecar.flags & BITRAF64_SIDECAR_NOT_DAG_DEPTH) != 0U);
}

static void test_manifest_mutation_detected(void)
{
    struct termux_manifest_entry_v2 entry;
    bitraf64_sidecar_v1_t sidecar;
    uint8_t data[] = {1,2,3,4,5,6,7,8,9,10};
    memset(&entry, 0, sizeof(entry));
    memset(&sidecar, 0, sizeof(sidecar));
    entry.dep_count = 1;
    entry.crc32c = 7U;
    entry.coherence_phi = 9U;
    assert(bitraf64_manifest_embed(&entry, &sidecar, data, sizeof(data), 7U));
    assert(bitraf64_manifest_validate(&entry, &sidecar, data, sizeof(data), 7U));
    data[3] ^= 0x80U;
    assert(!bitraf64_manifest_validate(&entry, &sidecar, data, sizeof(data), 7U));
}

static void test_claim_gate_fails_closed(void)
{
    struct termux_manifest_entry_v2 entry;
    bitraf64_sidecar_v1_t sidecar;
    bitraf64_claim_gate_t gate;
    const uint8_t data[] = {11,22,33,44,55};
    memset(&entry, 0, sizeof(entry));
    memset(&sidecar, 0, sizeof(sidecar));
    entry.dep_count = 0;
    entry.crc32c = 1U;
    entry.coherence_phi = 1U;
    assert(bitraf64_manifest_embed(&entry, &sidecar, data, sizeof(data), 100U));
    gate = bitraf64_claim_gate_collect(&entry, &sidecar, data, sizeof(data), 100U);
    assert(gate.torus_roundtrip_verified == 1U);
    assert(gate.manifest_tag_roundtrip_verified == 1U);
    assert(gate.gcd_claim_corrected == 1U);
    assert(gate.r_corr_formula_matches_declared == 0U);
    assert(gate.linear_invertibility_proven == 0U);
    assert(gate.single_error_correction_proven == 0U);
    assert(gate.real_dag_alignment_proven == 0U);
    assert(gate.entropy_autocorrelation_measured == 0U);
    assert(gate.simd_speedup_benchmarked == 0U);
    assert(gate.claim_allowed == 0U);
}

int main(void)
{
    test_all_coordinates_roundtrip();
    test_torus_wraparound();
    test_corrected_arithmetic();
    test_manifest_sidecar_no_silent_truncation();
    test_manifest_mutation_detected();
    test_claim_gate_fails_closed();
    puts("BITRAF64_PHASE1_PROOF_GATE_PASS");
    return 0;
}
