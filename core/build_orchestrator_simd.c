#include "build_orchestrator.h"
#include <string.h>

#define SIMD_VECTOR_WIDTH 4
#define SIMD_CACHE_LINE_SIZE 64

#ifdef __ARM_NEON
  #include <arm_neon.h>
  #define ENABLE_NEON 1
#else
  #define ENABLE_NEON 0
#endif

#ifdef __SSE2__
  #include <emmintrin.h>
  #define ENABLE_SSE 1
#else
  #define ENABLE_SSE 0
#endif

#ifdef __AVX2__
  #include <immintrin.h>
  #define ENABLE_AVX 1
#else
  #define ENABLE_AVX 0
#endif

typedef struct {
  uint32_t phase[SIMD_VECTOR_WIDTH];
  uint32_t arch_state[SIMD_VECTOR_WIDTH];
  uint32_t pkg_idx[SIMD_VECTOR_WIDTH];
  uint32_t cycle_count[SIMD_VECTOR_WIDTH];
  uint64_t coherence_phi[SIMD_VECTOR_WIDTH];
} simd_build_state_t;

typedef struct {
  uint32_t valid_mask;
  simd_build_state_t states;
} simd_batch_t;

static inline uint32_t gcd_compute_simd(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

static inline uint64_t phi_compute_simd(uint32_t phase, uint32_t arch_state,
                                        uint32_t cycle_count)
    __attribute__((unused));
static inline uint64_t phi_compute_simd(uint32_t phase, uint32_t arch_state,
                                        uint32_t cycle_count) {
  uint64_t overhead_penalty = cycle_count > 42 ? 1000ULL : 0ULL;
  uint64_t depth = phase * 6 + arch_state;
  uint64_t depth_score = 42ULL - depth;
  uint64_t coherence_base = (depth_score * 65536ULL) / 42;

  uint32_t gcd_val = gcd_compute_simd(phase + 1, 7);
  uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536ULL) / 7;

  uint64_t result = (coherence_base * gcd_factor / 65536ULL);
  return (result > overhead_penalty) ? (result - overhead_penalty) : 0;
}

#if ENABLE_NEON
static inline void compute_phi_neon_u32(uint32_t phases[4],
                                        uint32_t arch_states[4],
                                        uint32_t cycle_counts[4],
                                        uint64_t phi_out[4]) {
  uint32x4_t v_phase = vld1q_u32(phases);
  uint32x4_t v_arch = vld1q_u32(arch_states);
  uint32x4_t v_cycles = vld1q_u32(cycle_counts);

  uint32x4_t v_42 = vdupq_n_u32(42);
  uint32x4_t v_6 = vdupq_n_u32(6);

  uint32x4_t v_depth = vaddq_u32(vmulq_u32(v_phase, v_6), v_arch);
  uint32x4_t v_depth_score = vsubq_u32(v_42, v_depth);
  uint32x4_t v_phase_plus_1 = vaddq_u32(v_phase, vdupq_n_u32(1));

  /*
   * NEON vgetq_lane_* requires a compile-time immediate lane index on both
   * ARM32 and AArch64.  The previous variable-index loop therefore compiled
   * on neither strict cross toolchain.  Materialize the vectors once and keep
   * the scalar GCD/division tail byte-for-byte equivalent without pretending
   * a runtime lane index is legal NEON.
   */
  uint32_t depth_score[SIMD_VECTOR_WIDTH];
  uint32_t phase_plus_1[SIMD_VECTOR_WIDTH];
  uint32_t cycles[SIMD_VECTOR_WIDTH];
  vst1q_u32(depth_score, v_depth_score);
  vst1q_u32(phase_plus_1, v_phase_plus_1);
  vst1q_u32(cycles, v_cycles);

  for (int i = 0; i < SIMD_VECTOR_WIDTH; i++) {
    uint64_t coherence_base = ((uint64_t)depth_score[i] * 65536) / 42;
    uint32_t gcd_val = gcd_compute_simd(phase_plus_1[i], 7);
    uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536) / 7;
    uint64_t result = (coherence_base * gcd_factor / 65536);
    uint64_t overhead = cycles[i] > 42 ? 1000ULL : 0ULL;
    phi_out[i] = (result > overhead) ? (result - overhead) : 0;
  }
}
#endif

#if ENABLE_SSE
static inline void compute_phi_sse_u32(uint32_t phases[4],
                                       uint32_t arch_states[4],
                                       uint32_t cycle_counts[4],
                                       uint64_t phi_out[4]) {
  for (int i = 0; i < SIMD_VECTOR_WIDTH; i++) {
    uint32_t phase = phases[i];
    uint32_t arch = arch_states[i];
    uint32_t cycles = cycle_counts[i];

    uint64_t depth_score = 42 - (phase * 6 + arch);
    uint64_t coherence_base = (depth_score * 65536) / 42;
    uint32_t gcd_val = gcd_compute_simd(phase + 1, 7);
    uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536) / 7;
    uint64_t result = (coherence_base * gcd_factor / 65536);
    uint64_t overhead = cycles > 42 ? 1000ULL : 0ULL;
    phi_out[i] = (result > overhead) ? (result - overhead) : 0;
  }
}
#endif

#if ENABLE_AVX
static inline void compute_phi_avx2_u32(uint32_t phases[4],
                                        uint32_t arch_states[4],
                                        uint32_t cycle_counts[4],
                                        uint64_t phi_out[4]) {
  for (int i = 0; i < SIMD_VECTOR_WIDTH; i++) {
    uint32_t depth_s = 42 - (phases[i] * 6 + arch_states[i]);
    uint64_t coherence_base = ((uint64_t)depth_s * 65536) / 42;
    uint32_t gcd_val = gcd_compute_simd(phases[i] + 1, 7);
    uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536) / 7;
    uint64_t result = (coherence_base * gcd_factor / 65536);
    uint64_t overhead = cycle_counts[i] > 42 ? 1000ULL : 0ULL;
    phi_out[i] = (result > overhead) ? (result - overhead) : 0;
  }
}
#endif

static inline void simd_batch_init(simd_batch_t *batch, uint32_t count) {
  memset(batch, 0, sizeof(*batch));
  batch->valid_mask = (1 << count) - 1;
}

static inline void simd_batch_add_package(simd_batch_t *batch,
                                          uint32_t slot,
                                          uint32_t phase,
                                          uint32_t arch_state,
                                          uint32_t pkg_idx)
    __attribute__((unused));
static inline void simd_batch_add_package(simd_batch_t *batch,
                                          uint32_t slot,
                                          uint32_t phase,
                                          uint32_t arch_state,
                                          uint32_t pkg_idx) {
  if (slot >= SIMD_VECTOR_WIDTH) return;

  batch->states.phase[slot] = phase;
  batch->states.arch_state[slot] = arch_state;
  batch->states.pkg_idx[slot] = pkg_idx;
  batch->states.cycle_count[slot] = 0;
}

int termux_orchestrator_execute_simd_batch(struct termux_orchestrator *orch,
                                           simd_batch_t *batch,
                                           uint32_t batch_count) {
  if (!orch || !batch || batch_count == 0) return -1;
  if (batch_count > SIMD_VECTOR_WIDTH) batch_count = SIMD_VECTOR_WIDTH;

  uint64_t phi_results[SIMD_VECTOR_WIDTH];

#if ENABLE_NEON
  compute_phi_neon_u32(batch->states.phase,
                       batch->states.arch_state,
                       batch->states.cycle_count,
                       phi_results);
#elif ENABLE_AVX
  compute_phi_avx2_u32(batch->states.phase,
                       batch->states.arch_state,
                       batch->states.cycle_count,
                       phi_results);
#elif ENABLE_SSE
  compute_phi_sse_u32(batch->states.phase,
                      batch->states.arch_state,
                      batch->states.cycle_count,
                      phi_results);
#else
  for (uint32_t i = 0; i < batch_count; i++) {
    phi_results[i] = phi_compute_simd(batch->states.phase[i],
                                      batch->states.arch_state[i],
                                      batch->states.cycle_count[i]);
  }
#endif

  for (uint32_t i = 0; i < batch_count; i++) {
    batch->states.coherence_phi[i] = phi_results[i];
  }

  return 0;
}
