#ifndef TERMUX_BUILD_ORCHESTRATOR_SIMD_H
#define TERMUX_BUILD_ORCHESTRATOR_SIMD_H

#include <stdint.h>
#include "build_orchestrator.h"

#define SIMD_VECTOR_WIDTH 4
#define SIMD_CACHE_LINE_SIZE 64

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

int termux_orchestrator_execute_simd_4way(struct termux_orchestrator *orch,
                                          const char *pkg_names[4],
                                          uint32_t pkg_indices[4],
                                          uint32_t pkg_count);

#endif
