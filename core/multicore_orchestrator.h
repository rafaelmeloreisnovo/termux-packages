#ifndef TERMUX_MULTICORE_ORCHESTRATOR_H
#define TERMUX_MULTICORE_ORCHESTRATOR_H

#include "build_orchestrator.h"
#include "phase_barrier_lockfree.h"
#include <stdint.h>
#include <pthread.h>

#define TERMUX_MAX_CORES 8
#define TERMUX_CORE_AFFINITY_MASK 0xFF

typedef enum {
  CORE_CLASS_PERFORMANCE = 0,
  CORE_CLASS_EFFICIENCY = 1,
  CORE_CLASS_GENERIC = 2
} core_class_t;

typedef struct {
  uint32_t core_id;
  core_class_t core_class;
  uint32_t cpu_freq_mhz;
  uint32_t l1_cache_kb;
  uint32_t l2_cache_kb;
  uint64_t total_cycles;
  uint64_t total_phi;
  uint32_t packages_completed;
} core_profile_t;

typedef struct {
  uint32_t layer_id;
  uint32_t pkg_count;
  uint16_t pkg_indices[65];
  volatile uint64_t completion_bitmap;
} layer_batch_t;

typedef struct {
  pthread_t threads[TERMUX_MAX_CORES];
  struct termux_orchestrator orchestrators[TERMUX_MAX_CORES];
  core_profile_t core_profiles[TERMUX_MAX_CORES];
  struct termux_phase_barrier layer_barrier;

  layer_batch_t layers[TERMUX_DAG_LAYERS];
  uint32_t layer_count;
  uint32_t core_count;

  volatile uint32_t active_cores;
  volatile uint32_t layer_index;
  volatile uint64_t total_cycles;
  volatile uint64_t total_phi;

  double wall_time_sec;
  double speedup_vs_single;
  double efficiency;
} multicore_orchestrator_t;

int multicore_orchestrator_alloc(multicore_orchestrator_t *morch, uint32_t core_count);

void multicore_orchestrator_free(multicore_orchestrator_t *morch);

int multicore_orchestrator_init(multicore_orchestrator_t *morch);

int multicore_orchestrator_detect_cores(multicore_orchestrator_t *morch);

int multicore_orchestrator_execute_parallel(multicore_orchestrator_t *morch,
                                           const layer_batch_t *layers,
                                           uint32_t layer_count);

int multicore_orchestrator_load_balance(multicore_orchestrator_t *morch,
                                       uint32_t layer_idx);

double multicore_orchestrator_calculate_speedup(multicore_orchestrator_t *morch,
                                               uint64_t single_core_cycles);

void multicore_orchestrator_print_stats(const multicore_orchestrator_t *morch);

#endif
