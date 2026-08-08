#ifndef TERMUX_BUILD_ORCHESTRATOR_H
#define TERMUX_BUILD_ORCHESTRATOR_H

#include <stdint.h>
#include <stddef.h>

#define TERMUX_ORCHESTRATOR_PHASES 7
#define TERMUX_ORCHESTRATOR_ARCH_STATES 6
#define TERMUX_ORCHESTRATOR_TOTAL_STATES 42

#define TERMUX_ARCH_STATE_ARM32_BASE       0
#define TERMUX_ARCH_STATE_ARM32_NEON       1
#define TERMUX_ARCH_STATE_ARM32_NEON_CRC   2
#define TERMUX_ARCH_STATE_ARM64_BASE       3
#define TERMUX_ARCH_STATE_ARM64_SIMD_CRC   4
#define TERMUX_ARCH_STATE_X86_64_AVX2      5

#define TERMUX_PHASE_SETUP_VARS  0
#define TERMUX_PHASE_SOURCE      1
#define TERMUX_PHASE_PATCH       2
#define TERMUX_PHASE_CONFIGURE   3
#define TERMUX_PHASE_MAKE        4
#define TERMUX_PHASE_INSTALL     5
#define TERMUX_PHASE_PACKAGE     6

#define TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE 256
#define TERMUX_ORCHESTRATOR_STATE_SIZE 512

struct termux_build_state {
  uint32_t phase;           // 0..6
  uint32_t arch_state;      // 0..5
  uint32_t pkg_idx;         // package index
  char pkg_name[256];       // current package name
  uint64_t coherence_phi;   // φ score (Q48.16 fixed-point)
  uint32_t cycle_count;     // instruction counter (instrumentation)
  uint32_t flags;           // state flags (bit-packed)
  uint32_t _pad;
  uint8_t state_buffer[256]; // pre-allocated output buffer
} __attribute__((aligned(256)));

typedef int (*termux_orchestrator_phase_fn)(struct termux_build_state *);

struct termux_orchestrator {
  struct termux_build_state state;
  termux_orchestrator_phase_fn phase_handlers[TERMUX_ORCHESTRATOR_PHASES];
  uint64_t attractor_table[TERMUX_ORCHESTRATOR_TOTAL_STATES];
  uint64_t (*coherence_fn)(struct termux_build_state *);
};

int termux_orchestrator_init(struct termux_orchestrator *orch);
int termux_orchestrator_execute(struct termux_orchestrator *orch,
                                  const char *pkg_name,
                                  uint32_t pkg_idx);
int termux_orchestrator_transition(struct termux_orchestrator *orch);
uint64_t termux_orchestrator_compute_phi(struct termux_build_state *state);
int termux_orchestrator_validate_invariants(struct termux_build_state *state);

#endif // TERMUX_BUILD_ORCHESTRATOR_H
