#include "build_orchestrator_advanced_simd.h"
#include "build_orchestrator_simd.c"
#include <string.h>

#ifdef __AVX512F__
#include <immintrin.h>
#define ENABLE_AVX512 1
#else
#define ENABLE_AVX512 0
#endif

#ifdef __ARM_SVE_H__
#include <arm_sve.h>
#define ENABLE_SVE 1
#else
#define ENABLE_SVE 0
#endif

#if ENABLE_AVX512
static inline void compute_phi_avx512_u32(uint32_t phases[8],
                                          uint32_t arch_states[8],
                                          uint32_t cycle_counts[8],
                                          uint64_t phi_out[8]) {
  __m512i v_phase = _mm512_loadu_si512((__m512i *)phases);
  __m512i v_arch = _mm512_loadu_si512((__m512i *)arch_states);
  __m512i v_cycles = _mm512_loadu_si512((__m512i *)cycle_counts);

  __m512i v_42 = _mm512_set1_epi32(42);
  __m512i v_6 = _mm512_set1_epi32(6);

  __m512i v_depth = _mm512_add_epi32(_mm512_mullo_epi32(v_phase, v_6), v_arch);
  __m512i v_depth_score = _mm512_sub_epi32(v_42, v_depth);

  for (int i = 0; i < 8; i++) {
    uint32_t depth_s = 42 - (phases[i] * 6 + arch_states[i]);
    uint64_t coherence_base = ((uint64_t)depth_s * 65536) / 42;
    uint32_t gcd_val = 7;
    uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536) / 7;
    uint64_t result = (coherence_base * gcd_factor / 65536);
    uint64_t overhead = cycle_counts[i] > 42 ? 1000ULL : 0ULL;
    phi_out[i] = (result > overhead) ? (result - overhead) : 0;
  }
}
#endif

#if ENABLE_SVE
static inline void compute_phi_sve_u32(uint32_t vector_factor,
                                       uint32_t phases[],
                                       uint32_t arch_states[],
                                       uint32_t cycle_counts[],
                                       uint64_t phi_out[]) {
  for (uint32_t i = 0; i < vector_factor; i++) {
    uint32_t depth_s = 42 - (phases[i] * 6 + arch_states[i]);
    uint64_t coherence_base = ((uint64_t)depth_s * 65536) / 42;
    uint32_t gcd_val = 7;
    uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536) / 7;
    uint64_t result = (coherence_base * gcd_factor / 65536);
    uint64_t overhead = cycle_counts[i] > 42 ? 1000ULL : 0ULL;
    phi_out[i] = (result > overhead) ? (result - overhead) : 0;
  }
}
#endif

int termux_orchestrator_execute_avx512_8way(struct termux_orchestrator *orch,
                                             const char *pkg_names[8],
                                             uint32_t pkg_indices[8],
                                             uint32_t pkg_count) {
  if (!orch || !pkg_names || !pkg_indices || pkg_count == 0) {
    return -1;
  }

  if (pkg_count > TERMUX_SIMD_VF_AVX512) {
    pkg_count = TERMUX_SIMD_VF_AVX512;
  }

#if ENABLE_AVX512
  simd_avx512_state_t state;
  memset(&state, 0, sizeof(state));

  for (uint32_t i = 0; i < pkg_count; i++) {
    state.phase[i] = TERMUX_PHASE_SETUP;
    state.arch_state[i] = TERMUX_ARCH_STATE_X86_64;
    state.pkg_idx[i] = pkg_indices[i];
    state.cycle_count[i] = 0;
  }

  while (state.phase[0] < TERMUX_BUILD_PHASES) {
    for (uint32_t i = 0; i < pkg_count; i++) {
      if (state.phase[i] < TERMUX_BUILD_PHASES) {
        state.phase[i]++;
      }
    }

    uint64_t phi_results[8];
    compute_phi_avx512_u32(state.phase, state.arch_state,
                          state.cycle_count, phi_results);

    for (uint32_t i = 0; i < pkg_count; i++) {
      state.coherence_phi[i] = phi_results[i];
      state.cycle_count[i] += 42;
    }
  }

  struct termux_build_state *state_ptr = &orch->state;
  memset(state_ptr, 0, sizeof(*state_ptr));
  state_ptr->cycle_count = state.cycle_count[0] / TERMUX_SIMD_VF_AVX512;
  state_ptr->coherence_phi = state.coherence_phi[0];

  return 0;
#else
  return -2;
#endif
}

int termux_orchestrator_execute_sve_vectorized(struct termux_orchestrator *orch,
                                                uint32_t vector_factor) {
  if (!orch || vector_factor == 0 || vector_factor > TERMUX_SIMD_VF_SVE) {
    return -1;
  }

#if ENABLE_SVE
  uint32_t phases[TERMUX_SIMD_VF_SVE];
  uint32_t arch_states[TERMUX_SIMD_VF_SVE];
  uint32_t cycle_counts[TERMUX_SIMD_VF_SVE];
  uint64_t coherence_phi[TERMUX_SIMD_VF_SVE];

  memset(phases, 0, sizeof(phases));
  memset(arch_states, 0, sizeof(arch_states));
  memset(cycle_counts, 0, sizeof(cycle_counts));
  memset(coherence_phi, 0, sizeof(coherence_phi));

  for (uint32_t i = 0; i < vector_factor; i++) {
    phases[i] = TERMUX_PHASE_SETUP;
    arch_states[i] = TERMUX_ARCH_STATE_ARM64;
    cycle_counts[i] = 0;
  }

  while (phases[0] < TERMUX_BUILD_PHASES) {
    for (uint32_t i = 0; i < vector_factor; i++) {
      if (phases[i] < TERMUX_BUILD_PHASES) {
        phases[i]++;
      }
    }

    uint64_t phi_results[TERMUX_SIMD_VF_SVE];
    compute_phi_sve_u32(vector_factor, phases, arch_states,
                       cycle_counts, phi_results);

    for (uint32_t i = 0; i < vector_factor; i++) {
      coherence_phi[i] = phi_results[i];
      cycle_counts[i] += 42;
    }
  }

  struct termux_build_state *state = &orch->state;
  memset(state, 0, sizeof(*state));
  state->cycle_count = cycle_counts[0] / vector_factor;
  state->coherence_phi = coherence_phi[0];

  return 0;
#else
  return -2;
#endif
}

int termux_orchestrator_has_avx512_support(void) {
#if ENABLE_AVX512
  return 1;
#else
  return 0;
#endif
}

int termux_orchestrator_has_sve_support(void) {
#if ENABLE_SVE
  return 1;
#else
  return 0;
#endif
}

const char *termux_orchestrator_advanced_simd_backend(void) {
#if ENABLE_AVX512
  return "x86_64 AVX-512 (8-way)";
#elif ENABLE_SVE
  return "ARM SVE (scalable up to 16-way)";
#else
  return "No advanced SIMD (fallback to 4-way)";
#endif
}
