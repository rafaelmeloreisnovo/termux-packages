#include "build_orchestrator.h"

#define GCD_UNSAFE(a, b) do { \
  while (b) { uint32_t t = b; b = a % b; a = t; } \
} while(0)

#define PHI_SCALE_BITS 16
#define PHI_SCALE (1ULL << PHI_SCALE_BITS)

typedef struct {
  uint32_t phase;
  uint32_t arch_state;
  uint32_t pkg_idx;
  uint64_t coherence_phi;
  uint32_t cycle_count;
  uint32_t flags;
} orch_state_inline_t;

static inline void memzero(void *p, size_t len) {
  uint8_t *b = (uint8_t *)p;
  for (size_t i = 0; i < len; i++) b[i] = 0;
}

static inline void memcpy_inline(void *dst, const void *src, size_t len) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < len; i++) d[i] = s[i];
}

static inline uint32_t gcd_compute(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

static inline int invariant_gcd_valid(uint32_t depth) {
  uint32_t gcd_val = gcd_compute(depth, TERMUX_DAG_LAYERS);
  uint8_t valid_gcds[] = {1, 2, 3, 6, 7, 14, 21, 42};
  for (size_t i = 0; i < 8; i++) {
    if (gcd_val == valid_gcds[i]) return 1;
  }
  return 0;
}

static inline int invariant_phi_overflow(uint64_t phi) {
  return phi <= ((1ULL << 48) - 1) ? 1 : 0;
}

static inline int invariant_arch_bounds(uint32_t arch) {
  return arch < TERMUX_ARCH_STATES ? 1 : 0;
}

static inline int invariant_phase_bounds(uint32_t phase) {
  return phase < TERMUX_BUILD_PHASES ? 1 : 0;
}

int termux_orchestrator_validate_invariants(struct termux_build_state *state) {
  if (!state) return -1;

  uint32_t depth = state->phase * TERMUX_ARCH_STATES + state->arch_state;

  if (!invariant_gcd_valid(depth)) return -1;
  if (!invariant_phi_overflow(state->coherence_phi)) return -2;
  if (!invariant_arch_bounds(state->arch_state)) return -3;
  if (!invariant_phase_bounds(state->phase)) return -4;

  return 0;
}

uint64_t termux_orchestrator_compute_phi(struct termux_build_state *state) {
  if (!state) return 0;

  uint64_t overhead_penalty = state->cycle_count > 42 ? 1000ULL : 0ULL;
  uint64_t depth_score = 42ULL - (state->phase * TERMUX_ARCH_STATES + state->arch_state);
  uint64_t coherence_base = (depth_score * PHI_SCALE) / TERMUX_DAG_LAYERS;

  uint32_t gcd_val = gcd_compute(state->phase + 1, TERMUX_BUILD_PHASES);
  uint64_t gcd_factor = (((uint64_t)gcd_val) * PHI_SCALE) / TERMUX_BUILD_PHASES;

  uint64_t result = (coherence_base * gcd_factor / PHI_SCALE) - overhead_penalty;
  return result > ((1ULL << 48) - 1) ? ((1ULL << 48) - 1) : result;
}

static inline int phase_handler_noop(struct termux_build_state *state) {
  if (!state) return -1;
  state->coherence_phi = termux_orchestrator_compute_phi(state);
  return 0;
}

int termux_orchestrator_init(struct termux_orchestrator *orch) {
  if (!orch) return -1;

  memzero(orch, sizeof(*orch));

  for (size_t i = 0; i < TERMUX_BUILD_PHASES; i++) {
    orch->phase_handlers[i] = phase_handler_noop;
  }

  for (size_t i = 0; i < TERMUX_DAG_LAYERS; i++) {
    uint32_t golden_ratio_32 = 0x9E3779B9U;
    orch->attractor_table[i] = ((uint64_t)(golden_ratio_32 * (i + 1)) << 16) | i;
  }

  orch->coherence_fn = termux_orchestrator_compute_phi;

  return 0;
}

int termux_orchestrator_transition(struct termux_orchestrator *orch) {
  if (!orch) return -1;

  struct termux_build_state *state = &orch->state;

  int inv_check = termux_orchestrator_validate_invariants(state);
  if (inv_check != 0) return inv_check;

  if (state->phase >= TERMUX_BUILD_PHASES) {
    return 0;
  }

  termux_orchestrator_phase_fn handler = orch->phase_handlers[state->phase];
  if (!handler) return -1;

  int phase_ret = handler(state);
  if (phase_ret != 0) return phase_ret;

  state->coherence_phi = termux_orchestrator_compute_phi(state);

  uint32_t next_phase = state->phase + 1;
  if (next_phase < TERMUX_BUILD_PHASES) {
    state->phase = next_phase;
  } else {
    state->phase = TERMUX_BUILD_PHASES;
  }

  return 0;
}

int termux_orchestrator_execute(struct termux_orchestrator *orch,
                                 const char *pkg_name,
                                 uint32_t pkg_idx) {
  if (!orch || !pkg_name) return -1;

  struct termux_build_state *state = &orch->state;
  memzero(state, sizeof(*state));

  const char *name_ptr = pkg_name;
  for (size_t i = 0; i < 255 && name_ptr && *name_ptr; i++) {
    state->pkg_name[i] = *name_ptr;
    name_ptr++;
  }
  state->pkg_name[255] = '\0';

  state->pkg_idx = pkg_idx;
  state->phase = TERMUX_PHASE_SETUP;
  state->arch_state = TERMUX_ARCH_STATE_ARM64;

  while (state->phase < TERMUX_BUILD_PHASES) {
    int ret = termux_orchestrator_transition(orch);
    if (ret != 0) {
      return ret;
    }
    state->cycle_count += 42;
  }

  return 0;
}
