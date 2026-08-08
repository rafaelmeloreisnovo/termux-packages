#ifndef TERMUX_BUILD_ORCHESTRATOR_ADVANCED_SIMD_H
#define TERMUX_BUILD_ORCHESTRATOR_ADVANCED_SIMD_H

#include <stdint.h>
#include "build_orchestrator.h"

#define TERMUX_SIMD_VF_AVX512 8  // 8-way vectorization (AVX-512)
#define TERMUX_SIMD_VF_SVE 16    // Up to 16-way vectorization (ARM SVE)
#define TERMUX_SIMD_VF_NEON 4    // 4-way vectorization (ARM NEON)

typedef struct {
  uint32_t phase[TERMUX_SIMD_VF_AVX512];
  uint32_t arch_state[TERMUX_SIMD_VF_AVX512];
  uint32_t pkg_idx[TERMUX_SIMD_VF_AVX512];
  uint32_t cycle_count[TERMUX_SIMD_VF_AVX512];
  uint64_t coherence_phi[TERMUX_SIMD_VF_AVX512];
} simd_avx512_state_t;

typedef struct {
  uint32_t vf;  // Vector factor (4, 8, 16)
  uint32_t *phase;
  uint32_t *arch_state;
  uint32_t *pkg_idx;
  uint32_t *cycle_count;
  uint64_t *coherence_phi;
} simd_sve_state_t;

int termux_orchestrator_execute_avx512_8way(struct termux_orchestrator *orch,
                                             const char *pkg_names[8],
                                             uint32_t pkg_indices[8],
                                             uint32_t pkg_count);

int termux_orchestrator_execute_sve_vectorized(struct termux_orchestrator *orch,
                                                uint32_t vector_factor);

int termux_orchestrator_has_avx512_support(void);

int termux_orchestrator_has_sve_support(void);

const char *termux_orchestrator_advanced_simd_backend(void);

#endif
