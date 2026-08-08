#include "build_orchestrator.h"
#include "manifest_v2.h"
#include <stdio.h>
#include <string.h>

/*
 * Phase 9.4 structural/model fixture.
 *
 * This executable does NOT communicate with an Android device, does not inspect
 * an ARM CPU, and does not measure hardware cycles. It exercises deterministic
 * orchestrator/manifest invariants on the host where it is compiled.
 *
 * claim_allowed=false
 * physical_device_verified=false
 */

#define FIXTURE_PKG_COUNT 16
#define MODEL_CYCLE_BUDGET_MAX 336
#define STATE_FOOTPRINT_MAX 512

static int test_state_footprint(void) {
  printf("=== State Footprint Structural Check ===\n");
  struct termux_build_state state = {};
  size_t footprint = sizeof(state);
  printf("  termux_build_state size: %zu bytes\n", footprint);
  printf("  structural max: %u bytes\n", STATE_FOOTPRINT_MAX);
  int pass = footprint <= STATE_FOOTPRINT_MAX;
  printf("  Result: %s\n\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : -1;
}

static int test_orchestrator_model_budget(void) {
  printf("=== Orchestrator Model Budget Check ===\n");
  printf("  unit=internal_model_counter (NOT CPU hardware cycles)\n");

  struct termux_orchestrator orch = {};
  if (termux_orchestrator_init(&orch) != 0) return -1;

  uint32_t violations = 0;
  for (uint32_t pkg = 0; pkg < FIXTURE_PKG_COUNT; pkg++) {
    char pkg_name[64] = {};
    snprintf(pkg_name, sizeof(pkg_name), "fixture-pkg-%u", pkg);
    if (termux_orchestrator_execute(&orch, pkg_name, pkg) != 0) return -1;
    if (orch.state.cycle_count > MODEL_CYCLE_BUDGET_MAX) violations++;
  }
  printf("  packages=%u violations=%u budget=%u\n",
         FIXTURE_PKG_COUNT, violations, MODEL_CYCLE_BUDGET_MAX);
  printf("  Result: %s\n\n", violations == 0 ? "PASS" : "FAIL");
  return violations == 0 ? 0 : -1;
}

static int test_coherence_model_trajectory(void) {
  printf("=== Coherence Model Trajectory ===\n");
  struct termux_orchestrator orch = {};
  if (termux_orchestrator_init(&orch) != 0) return -1;

  uint64_t previous = 0;
  uint32_t decreases = 0;
  for (uint32_t phase = 0; phase < 8; phase++) {
    orch.state.phase = phase;
    orch.state.arch_state = 3;
    orch.state.cycle_count = 42 + phase * 5;
    uint64_t phi = termux_orchestrator_compute_phi(&orch.state);
    if (phase > 0 && phi < previous) decreases++;
    previous = phi;
    printf("  phase=%u model_phi=%lu\n", phase, phi);
  }
  printf("  decreases=%u/7\n", decreases);
  printf("  Observation only: no hardware/performance conclusion\n\n");
  return 0;
}

static int test_arch_state_domain(void) {
  printf("=== Architecture-State Domain Check ===\n");
  struct termux_orchestrator orch = {};
  if (termux_orchestrator_init(&orch) != 0) return -1;

  for (uint32_t arch = 0; arch < 4; arch++) {
    orch.state.arch_state = arch;
    orch.state.phase = 0;
    orch.state.cycle_count = 0;
    if (termux_orchestrator_validate_invariants(&orch.state) != 0) return -1;
  }
  printf("  model_states=4/4 PASS\n");
  printf("  cross_arch_execution=NOT_TESTED\n\n");
  return 0;
}

static int test_manifest_fixture(void) {
  printf("=== Manifest Structural Integration ===\n");
  struct termux_manifest_entry_v2 entries[FIXTURE_PKG_COUNT];
  memset(entries, 0, sizeof(entries));

  for (int i = 0; i < FIXTURE_PKG_COUNT; i++) {
    snprintf(entries[i].name, TERMUX_MANIFEST_PKG_NAME_LEN, "pkg-%d", i);
    entries[i].toroidal_depth = (uint32_t)i % 32U;
    termux_manifest_v2_entry_compute_phi(&entries[i], entries[i].toroidal_depth);
    termux_manifest_v2_entry_compute_crc32c(&entries[i]);
  }

  if (termux_manifest_v2_validate_all(entries, FIXTURE_PKG_COUNT) != 0)
    return -1;

  uint32_t crc = termux_manifest_v2_compute_global_crc32c(entries, FIXTURE_PKG_COUNT);
  printf("  entries=%u structural_validation=PASS global_crc32c=0x%08x\n",
         FIXTURE_PKG_COUNT, crc);
  printf("  package_buildability=NOT_TESTED\n\n");
  return 0;
}

static int test_phase_sequence(void) {
  printf("=== Phase-Sequence Model Check ===\n");
  struct termux_orchestrator orch = {};
  if (termux_orchestrator_init(&orch) != 0) return -1;

  uint32_t previous = 0;
  for (uint32_t pkg = 0; pkg < 8; pkg++) {
    char name[64] = {};
    snprintf(name, sizeof(name), "phase-fixture-%u", pkg);
    if (termux_orchestrator_execute(&orch, name, pkg) != 0) return -1;
    if (pkg > 0 && orch.state.phase < previous) return -1;
    previous = orch.state.phase;
  }
  printf("  sequence_regressions=0\n");
  printf("  synchronization_across_real_workers=NOT_TESTED\n\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf(" TERMUX-PACKAGES PHASE 9.4 STRUCTURAL/MODEL FIXTURE — NOT DEVICE VALIDATION\n");
  printf("================================================================================\n");
  printf("CLAIM_ALLOWED=false\n");
  printf("PHYSICAL_DEVICE_VERIFIED=false\n");
  printf("ARM32_RUNTIME=TOKEN_VAZIO\n");
  printf("ARM64_RUNTIME=TOKEN_VAZIO\n\n");

  int failures = 0;
  failures += test_state_footprint() != 0;
  failures += test_orchestrator_model_budget() != 0;
  failures += test_coherence_model_trajectory() != 0;
  failures += test_arch_state_domain() != 0;
  failures += test_manifest_fixture() != 0;
  failures += test_phase_sequence() != 0;

  printf("================================================================================\n");
  if (failures == 0) {
    printf("MODEL_FIXTURE_GATE=PASS\n");
    printf("DEVICE_RUNTIME=TOKEN_VAZIO_UNLESS_SEPARATE_RECEIPT\n");
    printf("PRODUCT_READINESS=NOT_CLAIMED\n");
  } else {
    printf("MODEL_FIXTURE_GATE=FAIL failures=%d\n", failures);
  }
  printf("================================================================================\n\n");
  return failures == 0 ? 0 : 1;
}
