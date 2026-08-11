#include "build_orchestrator.h"
#include <string.h>

#define PHI_SCALE_BITS 16
#define PHI_SCALE (1ULL << PHI_SCALE_BITS)
#define CACHE_LINE_SIZE 64
#define PREFETCH_DISTANCE 2

typedef struct {
  uint32_t phase;
  uint32_t arch_state;
  uint32_t pkg_idx;
  uint64_t coherence_phi;
  uint32_t cycle_count;
  uint32_t flags;
  uint8_t padding[CACHE_LINE_SIZE - 24];
} orch_state_cache_aligned_t;

static inline void prefetch(const void *addr) {
#ifdef __GNUC__
  __builtin_prefetch(addr, 0, 3);
#endif
}

static inline uint32_t gcd_compute_branchless(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

static inline int invariant_gcd_valid(uint32_t depth) {
  uint32_t gcd_val = gcd_compute_branchless(depth, TERMUX_DAG_LAYERS);
  uint8_t valid_gcds[] = {1, 2, 4, 8, 16, 32};
  for (size_t i = 0; i < 6; i++) {
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

static inline uint64_t phi_compute_inline(uint32_t phase, uint32_t arch_state,
                                          uint32_t cycle_count) {
  uint64_t overhead_penalty = cycle_count > 42 ? 1000ULL : 0ULL;
  uint64_t depth_score = 42ULL - (phase * TERMUX_ARCH_STATES + arch_state);
  uint64_t coherence_base = (depth_score * PHI_SCALE) / TERMUX_DAG_LAYERS;

  uint32_t gcd_val = gcd_compute_branchless(phase + 1, TERMUX_BUILD_PHASES);
  uint64_t gcd_factor = (((uint64_t)gcd_val) * PHI_SCALE) / TERMUX_BUILD_PHASES;

  uint64_t result = (coherence_base * gcd_factor / PHI_SCALE) - overhead_penalty;
  return result > ((1ULL << 48) - 1) ? ((1ULL << 48) - 1) : result;
}

int termux_orchestrator_init_optimized(struct termux_orchestrator *orch) {
  if (!orch) return -1;

  memset(orch, 0, sizeof(*orch));

  for (size_t i = 0; i < TERMUX_BUILD_PHASES; i++) {
    orch->phase_handlers[i] = NULL;
  }

  for (size_t i = 0; i < TERMUX_DAG_LAYERS; i++) {
    uint32_t golden_ratio_32 = 0x9E3779B9U;
    orch->attractor_table[i] = ((uint64_t)(golden_ratio_32 * (i + 1)) << 16) | i;
  }

  orch->coherence_fn = NULL;

  return 0;
}

int termux_orchestrator_execute_optimized(struct termux_orchestrator *orch,
                                          const char *pkg_name,
                                          uint32_t pkg_idx) {
  if (!orch || !pkg_name) return -1;

  struct termux_build_state *state = &orch->state;
  memset(state, 0, sizeof(*state));

  const char *name_ptr = pkg_name;
  size_t name_idx = 0;
  while (name_idx < 255 && name_ptr && *name_ptr) {
    state->pkg_name[name_idx++] = *name_ptr++;
  }
  state->pkg_name[name_idx] = '\0';

  state->pkg_idx = pkg_idx;
  state->phase = TERMUX_PHASE_SETUP;
  state->arch_state = TERMUX_ARCH_STATE_ARM64;

  while (state->phase < TERMUX_BUILD_PHASES) {
    uint32_t depth = state->phase * TERMUX_ARCH_STATES + state->arch_state;

    if (!invariant_gcd_valid(depth)) return -1;
    if (!invariant_phi_overflow(state->coherence_phi)) return -2;
    if (!invariant_arch_bounds(state->arch_state)) return -3;
    if (!invariant_phase_bounds(state->phase)) return -4;

    state->coherence_phi = phi_compute_inline(state->phase, state->arch_state, state->cycle_count);

    uint32_t next_phase = state->phase + 1;
    state->phase = (next_phase < TERMUX_BUILD_PHASES) ? next_phase : TERMUX_BUILD_PHASES;

    state->cycle_count += 42;
  }

  return 0;
}

static inline void transition_batch_optimized(struct termux_build_state *state,
                                              uint32_t batch_size) {
  for (uint32_t i = 0; i < batch_size; i++) {
    state->coherence_phi = phi_compute_inline(state->phase, state->arch_state, state->cycle_count);

    if (state->phase < TERMUX_BUILD_PHASES - 1) {
      state->phase++;
    } else {
      state->phase = TERMUX_BUILD_PHASES;
      break;
    }
  }
}

int termux_orchestrator_execute_batched(struct termux_orchestrator *orch,
                                        const char *pkg_name,
                                        uint32_t pkg_idx) {
  if (!orch || !pkg_name) return -1;

  struct termux_build_state *state = &orch->state;
  memset(state, 0, sizeof(*state));

  const char *name_ptr = pkg_name;
  size_t name_idx = 0;
  while (name_idx < 255 && name_ptr && *name_ptr) {
    state->pkg_name[name_idx++] = *name_ptr++;
  }
  state->pkg_name[name_idx] = '\0';

  state->pkg_idx = pkg_idx;
  state->phase = TERMUX_PHASE_SETUP;
  state->arch_state = TERMUX_ARCH_STATE_ARM64;

  while (state->phase < TERMUX_BUILD_PHASES) {
    prefetch((const void *)state);

    transition_batch_optimized(state, 1);
    state->cycle_count += 42;
  }

  return 0;
}

static inline uint32_t phase_transition_count(void) __attribute__((unused));
static inline uint32_t phase_transition_count(void) {
  return TERMUX_BUILD_PHASES;
}

static inline void warmup_cache(struct termux_orchestrator *orch) {
  struct termux_build_state *state = &orch->state;
  memset(state, 0, sizeof(*state));

  for (uint32_t i = 0; i < TERMUX_DAG_LAYERS; i++) {
    prefetch(&orch->attractor_table[i]);
  }
}

int termux_orchestrator_execute_warmup(struct termux_orchestrator *orch,
                                       const char *pkg_name,
                                       uint32_t pkg_idx) {
  warmup_cache(orch);

  return termux_orchestrator_execute_optimized(orch, pkg_name, pkg_idx);
}

static inline uint64_t measure_cycle_latency(uint32_t phase, uint32_t arch_state) {
  uint64_t depth_score = 42ULL - (phase * TERMUX_ARCH_STATES + arch_state);
  uint64_t coherence_base = (depth_score * PHI_SCALE) / TERMUX_DAG_LAYERS;

  uint32_t gcd_val = gcd_compute_branchless(phase + 1, TERMUX_BUILD_PHASES);
  uint64_t gcd_factor = (((uint64_t)gcd_val) * PHI_SCALE) / TERMUX_BUILD_PHASES;

  return (coherence_base * gcd_factor / PHI_SCALE);
}

uint32_t termux_orchestrator_estimate_overhead(struct termux_orchestrator *orch) {
  if (!orch) return 0;

  uint32_t total_cycles = 0;
  for (uint32_t phase = 0; phase < TERMUX_BUILD_PHASES; phase++) {
    for (uint32_t arch = 0; arch < TERMUX_ARCH_STATES; arch++) {
      uint64_t phi = measure_cycle_latency(phase, arch);
      total_cycles += (phi > 0 ? 1 : 0) + 42;
    }
  }

  return total_cycles / TERMUX_DAG_LAYERS;
}

double termux_orchestrator_calculate_efficiency(uint32_t wall_cycles,
                                                uint32_t overhead_cycles) {
  if (overhead_cycles == 0 || wall_cycles == 0) return 0.0;

  double overhead_ratio = (double)overhead_cycles / (double)wall_cycles;
  return 1.0 - overhead_ratio;
}

uint64_t termux_orchestrator_predict_phi(uint32_t phase, uint32_t arch_state,
                                         uint32_t measured_cycles,
                                         uint32_t baseline_cycles) {
  uint64_t overhead_penalty = measured_cycles > baseline_cycles ? 1000ULL : 0ULL;
  uint64_t base_phi = phi_compute_inline(phase, arch_state, baseline_cycles);

  return (base_phi > overhead_penalty) ? (base_phi - overhead_penalty) : 0ULL;
}
