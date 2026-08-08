#ifndef TERMUX_BUILD_ORCHESTRATOR_V2_H
#define TERMUX_BUILD_ORCHESTRATOR_V2_H

#include <stdint.h>
#include <stddef.h>

/*
 * Phase 3: Build Orchestrator V2
 * Zero-Abstraction State Machine with 42 Deterministic States
 *
 * Topology: 6×7 = 42 states
 *   - 6 Architecture States: ARM32-base, ARM32-NEON, ARM32-CRC32c, ARM64-base, ARM64-SIMD, x86_64-AVX2
 *   - 7 Build Phases: SETUP, SOURCE, PATCH, CONFIGURE, COMPILE, INSTALL, PACKAGE
 *
 * Design Principles:
 *   - Zero malloc in hot paths (stack-allocated state)
 *   - Deterministic transitions (lookup table, no branches)
 *   - Nanosecond-precision timing for coherence tracking
 *   - Pre-allocated circular buffers for output
 */

#define TERMUX_BUILD_ORCHESTRATOR_V2_VERSION "3.0.0"
#define TERMUX_MAX_PACKAGES 2057
#define TERMUX_STATE_BUFFER_SIZE 256
#define TERMUX_MAX_PHASES 7
#define TERMUX_MAX_ARCHS 6

/* ============================================================================
 * Enumerated Types
 * ============================================================================ */

typedef enum {
  BUILD_PHASE_SETUP = 0,      /* Setup (registradores, flags, ABI) */
  BUILD_PHASE_SOURCE = 1,     /* Get source + hash verification */
  BUILD_PHASE_PATCH = 2,      /* Apply patches (determinístico) */
  BUILD_PHASE_CONFIGURE = 3,  /* autotools/cmake dispatch */
  BUILD_PHASE_COMPILE = 4,    /* make -jN (paralelização) */
  BUILD_PHASE_INSTALL = 5,    /* DESTDIR staging */
  BUILD_PHASE_PACKAGE = 6,    /* .deb creation + manifest */
} build_phase_t;

typedef enum {
  ARCH_STATE_ARM32_BASE = 0,     /* ARM32 baseline (armeabi-v7a) */
  ARCH_STATE_ARM32_NEON = 1,     /* ARM32 + NEON (128-bit) */
  ARCH_STATE_ARM32_CRC32C = 2,   /* ARM32 + NEON + CRC32c */
  ARCH_STATE_ARM64_BASE = 3,     /* ARM64 baseline (aarch64) */
  ARCH_STATE_ARM64_SIMD = 4,     /* ARM64 + SIMD + CRC32c (full) */
  ARCH_STATE_X86_64_AVX2 = 5,    /* x86_64 + AVX2 (desktop) */
} arch_state_t;

/* ============================================================================
 * Core Data Structures
 * ============================================================================ */

typedef struct {
  uint32_t phase;               /* Current phase (0..6) */
  uint32_t arch_state;          /* Current arch state (0..5) */
  uint32_t pkg_idx;             /* Package index (0..2056) */
  char pkg_name[64];            /* Package name */
  char version[32];             /* Package version */
  uint64_t coherence_phi;       /* Q48.16 format coherence score */
  uint32_t cycle_count;         /* CPU cycles for instrumentation */
  uint64_t timestamp_ns;        /* Nanosecond timestamp */
  uint8_t state_buffer[TERMUX_STATE_BUFFER_SIZE]; /* Pre-allocated I/O */
} build_state_v2_t;

typedef struct {
  build_phase_t phase;
  arch_state_t arch_state;
  uint32_t pkg_idx;
  uint64_t elapsed_ns;          /* Time in this phase */
  uint64_t coherence_phi;       /* φ at end of phase */
  uint32_t exit_code;           /* 0 = success, other = failure */
} phase_result_t;

typedef struct {
  char name[32];                /* Architecture name */
  char abi[32];                 /* ABI identifier */
  uint32_t features;            /* Feature flags (NEON, SIMD, etc) */
  uint32_t cache_size;          /* L1D cache size */
  uint32_t prefetch_distance;   /* Prefetch hint in instructions */
} arch_config_t;

/* ============================================================================
 * State Machine Transitions
 * ============================================================================ */

/* Toroidal index calculation: (phase × 6) + arch_state = 0..41 */
static inline uint32_t toroidal_index(build_phase_t phase, arch_state_t arch) {
  return ((uint32_t)phase * TERMUX_MAX_ARCHS) + (uint32_t)arch;
}

/* Next phase transition (deterministic, no branches) */
static inline build_phase_t next_phase(build_phase_t current_phase) {
  return (build_phase_t)((current_phase + 1) % TERMUX_MAX_PHASES);
}

/* ============================================================================
 * Core API
 * ============================================================================ */

/* Initialize orchestrator state for package build */
int build_orchestrator_v2_init(build_state_v2_t *state,
                               uint32_t pkg_idx,
                               const char *pkg_name,
                               const char *version);

/* Execute single phase in state machine */
int build_orchestrator_v2_phase(build_state_v2_t *state,
                                const char *build_script,
                                phase_result_t *result);

/* Transition to next phase (deterministic) */
int build_orchestrator_v2_next_phase(build_state_v2_t *state);

/* Execute full build (all 7 phases) for package */
int build_orchestrator_v2_execute(build_state_v2_t *state,
                                  const char *build_script);

/* Get architecture configuration */
const arch_config_t* build_orchestrator_v2_get_arch_config(arch_state_t arch);

/* Get phase name as string */
const char* build_orchestrator_v2_get_phase_name(build_phase_t phase);

/* Get architecture name as string */
const char* build_orchestrator_v2_get_arch_name(arch_state_t arch);

/* Update coherence metric (Φ) based on phase results */
uint64_t build_orchestrator_v2_update_coherence(build_state_v2_t *state,
                                                const phase_result_t *result);

/* Generate human-readable state report */
int build_orchestrator_v2_report(const build_state_v2_t *state,
                                 char *buffer,
                                 size_t buffer_size);

#endif /* TERMUX_BUILD_ORCHESTRATOR_V2_H */
