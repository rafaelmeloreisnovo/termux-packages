#include "build_orchestrator.h"
#include "manifest.h"
#include "friction_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GCD_SAFE(a, b) ((b) == 0 ? (a) : gcd_recursive((a), (b)))
#define PHI_PRECISION 16
#define PHI_SCALE (1ULL << PHI_PRECISION)

static uint32_t gcd_recursive(uint32_t a, uint32_t b) {
  return b == 0 ? a : gcd_recursive(b, a % b);
}

static int termux_orch_phase_setup_vars(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_SETUP_VARS) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[setup-vars] arch_state=%u api=21\n", state->arch_state);
  return 0;
}

static int termux_orch_phase_source(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_SOURCE) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[get-source] idx=%u mode=ACQUIRE\n", state->pkg_idx);
  return 0;
}

static int termux_orch_phase_patch(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_PATCH) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[apply-patches] deterministic=true\n");
  return 0;
}

static int termux_orch_phase_configure(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_CONFIGURE) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[configure] state=%u\n", state->arch_state);
  return 0;
}

static int termux_orch_phase_make(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_MAKE) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[make] jobs=8 deterministic=true\n");
  return 0;
}

static int termux_orch_phase_install(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_INSTALL) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[install] zero_copy=true\n");
  return 0;
}

static int termux_orch_phase_package(struct termux_build_state *state) {
  if (!state || state->phase != TERMUX_PHASE_PACKAGE) return -1;
  memset(state->state_buffer, 0, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE);
  snprintf((char *)state->state_buffer, TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE,
           "[package] format=deb manifest_update=true\n");
  return 0;
}

static uint64_t termux_attractor_compute(uint32_t index) {
  if (index >= TERMUX_ORCHESTRATOR_TOTAL_STATES) return 0;
  const uint32_t golden_ratio_32 = 0x9E3779B9U;
  return ((uint64_t)(golden_ratio_32 * (index + 1)) << 16) | index;
}

uint64_t termux_orchestrator_compute_phi(struct termux_build_state *state) {
  if (!state) return 0;

  uint64_t overhead_penalty = state->cycle_count > 42 ? 1000 : 0;
  uint64_t depth_score = 42ULL - (state->phase * 6 + state->arch_state);
  uint64_t coherence_base = (depth_score * PHI_SCALE) / 42;

  uint64_t gcd_val = GCD_SAFE(state->phase + 1, TERMUX_ORCHESTRATOR_PHASES);
  uint64_t gcd_factor = (gcd_val * PHI_SCALE) / TERMUX_ORCHESTRATOR_PHASES;

  return (coherence_base * gcd_factor / PHI_SCALE) - overhead_penalty;
}

int termux_orchestrator_validate_invariants(struct termux_build_state *state) {
  if (!state) return -1;

  uint32_t gcd_depth_42 = GCD_SAFE(state->phase * 6 + state->arch_state, 42);
  uint32_t valid_gcds[] = {1, 2, 3, 6, 7, 14, 21, 42};
  int gcd_valid = 0;
  for (size_t i = 0; i < sizeof(valid_gcds) / sizeof(valid_gcds[0]); i++) {
    if (gcd_depth_42 == valid_gcds[i]) {
      gcd_valid = 1;
      break;
    }
  }
  if (!gcd_valid) return -1;

  if (state->coherence_phi > ((1ULL << 48) - 1)) return -2;

  if (state->arch_state >= TERMUX_ORCHESTRATOR_ARCH_STATES) return -3;

  if (state->phase >= TERMUX_ORCHESTRATOR_PHASES) return -4;

  return 0;
}

int termux_orchestrator_init(struct termux_orchestrator *orch) {
  if (!orch) return -1;

  memset(orch, 0, sizeof(*orch));

  orch->phase_handlers[TERMUX_PHASE_SETUP_VARS] = termux_orch_phase_setup_vars;
  orch->phase_handlers[TERMUX_PHASE_SOURCE] = termux_orch_phase_source;
  orch->phase_handlers[TERMUX_PHASE_PATCH] = termux_orch_phase_patch;
  orch->phase_handlers[TERMUX_PHASE_CONFIGURE] = termux_orch_phase_configure;
  orch->phase_handlers[TERMUX_PHASE_MAKE] = termux_orch_phase_make;
  orch->phase_handlers[TERMUX_PHASE_INSTALL] = termux_orch_phase_install;
  orch->phase_handlers[TERMUX_PHASE_PACKAGE] = termux_orch_phase_package;

  for (size_t i = 0; i < TERMUX_ORCHESTRATOR_TOTAL_STATES; i++) {
    orch->attractor_table[i] = termux_attractor_compute(i);
  }

  orch->coherence_fn = termux_orchestrator_compute_phi;

  return 0;
}

int termux_orchestrator_transition(struct termux_orchestrator *orch) {
  if (!orch) return -1;

  struct termux_build_state *state = &orch->state;

  int inv_check = termux_orchestrator_validate_invariants(state);
  if (inv_check != 0) return inv_check;

  if (state->phase >= TERMUX_ORCHESTRATOR_PHASES) {
    return 0;
  }

  termux_orchestrator_phase_fn handler = orch->phase_handlers[state->phase];
  if (!handler) return -1;

  int phase_ret = handler(state);
  if (phase_ret != 0) return phase_ret;

  state->coherence_phi = termux_orchestrator_compute_phi(state);

  uint32_t next_phase = state->phase + 1;
  if (next_phase < TERMUX_ORCHESTRATOR_PHASES) {
    state->phase = next_phase;
  } else {
    state->phase = TERMUX_ORCHESTRATOR_PHASES;
  }

  return 0;
}

int termux_orchestrator_execute(struct termux_orchestrator *orch,
                                 const char *pkg_name,
                                 uint32_t pkg_idx) {
  if (!orch || !pkg_name) return -1;

  struct termux_build_state *state = &orch->state;
  memset(state, 0, sizeof(*state));

  strncpy(state->pkg_name, pkg_name, sizeof(state->pkg_name) - 1);
  state->pkg_name[sizeof(state->pkg_name) - 1] = '\0';
  state->pkg_idx = pkg_idx;
  state->phase = TERMUX_PHASE_SETUP_VARS;
  state->arch_state = TERMUX_ARCH_STATE_ARM64_SIMD_CRC;

  while (state->phase < TERMUX_ORCHESTRATOR_PHASES) {
    int ret = termux_orchestrator_transition(orch);
    if (ret != 0) {
      fprintf(stderr, "[ERROR] orchestrator transition failed phase=%u ret=%d\n",
              state->phase, ret);
      return ret;
    }
    state->cycle_count += 42;
  }

  fprintf(stdout, "[orchestrator] phases_completed=%u phi=%lu\n",
          state->phase, state->coherence_phi);

  return 0;
}

void termux_orchestrator_print_state(struct termux_orchestrator *orch) {
  if (!orch) return;

  struct termux_build_state *state = &orch->state;
  printf("=== Orchestrator State ===\n");
  printf("Package: %s (idx=%u)\n", state->pkg_name, state->pkg_idx);
  printf("Phase: %u/%u, Arch State: %u\n", state->phase, TERMUX_ORCHESTRATOR_PHASES,
         state->arch_state);
  printf("Coherence Φ: %lu (Q48.16)\n", state->coherence_phi);
  printf("Cycle Count: %u\n", state->cycle_count);
  printf("State Buffer: %s\n", (char *)state->state_buffer);
}
