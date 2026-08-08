#ifndef TERMUX_PROFILER_H
#define TERMUX_PROFILER_H

#include "build_orchestrator.h"
#include "cycle_budget.h"
#include "coherence_tuning.h"
#include "workload_generator.h"
#include <stdint.h>
#include <time.h>

typedef struct {
  char build_id[64];
  uint64_t timestamp_start;
  double wall_time_sec;

  cycle_profile_t cycle_prof;
  cache_metrics_t cache_metrics;
  layer_coherence_t coherence_layers[TERMUX_DAG_LAYERS];

  uint32_t total_packages;
  uint32_t packages_completed;
  uint32_t packages_failed;

  double throughput_pkgs_per_sec;
  double total_build_time_hours;
  double speedup_vs_baseline;
} profiler_session_t;

int profiler_session_init(profiler_session_t *session, const char *build_id);

int profiler_session_start(profiler_session_t *session);

int profiler_session_record_package(profiler_session_t *session,
                                   uint32_t pkg_idx,
                                   const cycle_profile_t *cycles);

int profiler_session_finalize(profiler_session_t *session);

void profiler_session_print_summary(const profiler_session_t *session);

void profiler_session_print_timeline(const profiler_session_t *session);

void profiler_session_export_json(const profiler_session_t *session,
                                 const char *filename);

#endif
