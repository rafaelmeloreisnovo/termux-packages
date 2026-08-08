#ifndef TERMUX_BUILD_ORCHESTRATOR_H
#define TERMUX_BUILD_ORCHESTRATOR_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#define TERMUX_BUILD_PHASES 8
#define TERMUX_ARCH_STATES 4
#define TERMUX_DAG_LAYERS 32

#define TERMUX_ARCH_STATE_ARM32          0
#define TERMUX_ARCH_STATE_ARM64          1
#define TERMUX_ARCH_STATE_X86_64         2
#define TERMUX_ARCH_STATE_GENERIC        3

#define TERMUX_PHASE_SETUP      0
#define TERMUX_PHASE_SOURCE     1
#define TERMUX_PHASE_PATCH      2
#define TERMUX_PHASE_CONFIGURE  3
#define TERMUX_PHASE_MAKE       4
#define TERMUX_PHASE_INSTALL    5
#define TERMUX_PHASE_MASSAGE    6
#define TERMUX_PHASE_PACKAGE    7

#define TERMUX_ORCHESTRATOR_STATE_BUFFER_SIZE 256
#define TERMUX_ORCHESTRATOR_STATE_SIZE 512

struct termux_build_state {
  uint32_t phase;           // 0..7 (TERMUX_BUILD_PHASES)
  uint32_t arch_state;      // 0..3 (TERMUX_ARCH_STATES)
  uint32_t pkg_idx;         // package index 0..2056
  char pkg_name[256];       // current package name (256 bytes)
  uint64_t coherence_phi;   // φ score (Q48.16 fixed-point)
  uint32_t cycle_count;     // instruction counter
  uint32_t flags;           // state flags (bit-packed)
  uint32_t _pad0;
  uint8_t state_buffer[256]; // pre-allocated output buffer (256 bytes)
  uint8_t _pad1[4];         // alignment padding to 512 bytes
} __attribute__((aligned(512)));

_Static_assert(sizeof(struct termux_build_state) == 512,
               "termux_build_state must be exactly 512 bytes");
_Static_assert(offsetof(struct termux_build_state, pkg_name) == 16,
               "pkg_name offset must be 16 bytes");
_Static_assert(offsetof(struct termux_build_state, coherence_phi) == 272,
               "coherence_phi offset must be 272 bytes");
_Static_assert(offsetof(struct termux_build_state, state_buffer) == 288,
               "state_buffer offset must be 288 bytes");

typedef int (*termux_orchestrator_phase_fn)(struct termux_build_state *);

struct termux_orchestrator {
  struct termux_build_state state;
  termux_orchestrator_phase_fn phase_handlers[TERMUX_BUILD_PHASES];
  uint64_t attractor_table[TERMUX_DAG_LAYERS];
  uint64_t (*coherence_fn)(struct termux_build_state *);
};

_Static_assert(sizeof(uint64_t) * TERMUX_DAG_LAYERS == 256,
               "attractor_table must fit in 256 bytes (32 × 8)");

int termux_orchestrator_init(struct termux_orchestrator *orch);
int termux_orchestrator_execute(struct termux_orchestrator *orch,
                                  const char *pkg_name,
                                  uint32_t pkg_idx);
int termux_orchestrator_transition(struct termux_orchestrator *orch);
uint64_t termux_orchestrator_compute_phi(struct termux_build_state *state);
int termux_orchestrator_validate_invariants(struct termux_build_state *state);

int termux_orchestrator_init_optimized(struct termux_orchestrator *orch);
int termux_orchestrator_execute_optimized(struct termux_orchestrator *orch,
                                           const char *pkg_name,
                                           uint32_t pkg_idx);
int termux_orchestrator_execute_batched(struct termux_orchestrator *orch,
                                        const char *pkg_name,
                                        uint32_t pkg_idx);
int termux_orchestrator_execute_warmup(struct termux_orchestrator *orch,
                                       const char *pkg_name,
                                       uint32_t pkg_idx);
uint32_t termux_orchestrator_estimate_overhead(struct termux_orchestrator *orch);
double termux_orchestrator_calculate_efficiency(uint32_t wall_cycles,
                                                uint32_t overhead_cycles);
uint64_t termux_orchestrator_predict_phi(uint32_t phase, uint32_t arch_state,
                                         uint32_t measured_cycles,
                                         uint32_t baseline_cycles);

int termux_orchestrator_execute_simd_4way(struct termux_orchestrator *orch,
                                          const char *pkg_names[4],
                                          uint32_t pkg_indices[4],
                                          uint32_t pkg_count);
uint32_t termux_orchestrator_estimate_simd_speedup(uint32_t baseline_cycles);
int termux_orchestrator_has_simd_support(void);
const char *termux_orchestrator_simd_backend(void);

#endif // TERMUX_BUILD_ORCHESTRATOR_H
