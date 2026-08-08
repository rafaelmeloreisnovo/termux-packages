#include "../build_orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int test_orchestrator_init(void) {
  struct termux_orchestrator orch = {};
  int ret = termux_orchestrator_init(&orch);
  assert(ret == 0);
  assert(orch.phase_handlers[TERMUX_PHASE_SETUP] != NULL);
  assert(orch.attractor_table[0] != 0);
  printf("✓ test_orchestrator_init passed\n");
  return 0;
}

static int test_orchestrator_invariants_gcd(void) {
  struct termux_build_state state = {};
  state.phase = 0;
  state.arch_state = 0;
  int ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == 0);

  state.phase = 1;
  state.arch_state = 1;
  ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == 0);

  state.phase = 7;
  state.arch_state = 3;
  ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == 0);

  printf("✓ test_orchestrator_invariants_gcd passed (all depth/32 gcd valid)\n");
  return 0;
}

static int test_orchestrator_invariants_phi_overflow(void) {
  struct termux_build_state state = {};
  state.phase = 0;
  state.arch_state = 0;
  state.coherence_phi = (1ULL << 48) - 1;
  int ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == 0);

  state.coherence_phi = (1ULL << 48);
  ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == -2);

  printf("✓ test_orchestrator_invariants_phi_overflow passed\n");
  return 0;
}

static int test_orchestrator_invariants_arch_bounds(void) {
  struct termux_build_state state = {};
  state.phase = 0;
  state.arch_state = TERMUX_ARCH_STATES;
  int ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == -3);

  state.arch_state = TERMUX_ARCH_STATES - 1;
  ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == 0);

  printf("✓ test_orchestrator_invariants_arch_bounds passed\n");
  return 0;
}

static int test_orchestrator_invariants_phase_bounds(void) {
  struct termux_build_state state = {};
  state.arch_state = 0;
  state.phase = TERMUX_BUILD_PHASES;
  int ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == -4);

  state.phase = TERMUX_BUILD_PHASES - 1;
  ret = termux_orchestrator_validate_invariants(&state);
  assert(ret == 0);

  printf("✓ test_orchestrator_invariants_phase_bounds passed\n");
  return 0;
}

static int test_orchestrator_compute_phi(void) {
  struct termux_build_state state = {};
  state.phase = 0;
  state.arch_state = 0;
  state.cycle_count = 42;
  uint64_t phi = termux_orchestrator_compute_phi(&state);
  assert(phi > 0);

  state.cycle_count = 100;
  uint64_t phi_degraded = termux_orchestrator_compute_phi(&state);
  assert(phi_degraded < phi);

  printf("✓ test_orchestrator_compute_phi passed (phi computation and degradation verified)\n");
  return 0;
}

static int test_orchestrator_toroidal_coverage(void) {
  for (uint32_t phase = 0; phase < TERMUX_BUILD_PHASES; phase++) {
    for (uint32_t arch = 0; arch < TERMUX_ARCH_STATES; arch++) {
      struct termux_build_state state = {
        .phase = phase,
        .arch_state = arch,
        .coherence_phi = 1000,
        .cycle_count = 42,
      };
      int ret = termux_orchestrator_validate_invariants(&state);
      assert(ret == 0);
    }
  }

  printf("✓ test_orchestrator_toroidal_coverage passed (all 32 states valid)\n");
  return 0;
}

static int test_orchestrator_transition(void) {
  struct termux_orchestrator orch = {};
  int ret = termux_orchestrator_init(&orch);
  assert(ret == 0);

  ret = termux_orchestrator_execute(&orch, "test-package", 42);
  assert(ret == 0);
  assert(orch.state.phase == TERMUX_BUILD_PHASES);
  assert(orch.state.coherence_phi > 0);

  printf("✓ test_orchestrator_transition passed (full build cycle)\n");
  return 0;
}

int main(void) {
  printf("=== Build Orchestrator Unit Tests ===\n\n");

  int all_passed = 0;
  all_passed += test_orchestrator_init();
  all_passed += test_orchestrator_invariants_gcd();
  all_passed += test_orchestrator_invariants_phi_overflow();
  all_passed += test_orchestrator_invariants_arch_bounds();
  all_passed += test_orchestrator_invariants_phase_bounds();
  all_passed += test_orchestrator_compute_phi();
  all_passed += test_orchestrator_toroidal_coverage();
  all_passed += test_orchestrator_transition();

  printf("\n=== All tests passed! ===\n");
  return all_passed == 0 ? 0 : 1;
}
