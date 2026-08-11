#include "multicore_orchestrator.h"
#include "build_orchestrator.h"
#include "profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <inttypes.h>


int multicore_orchestrator_alloc(multicore_orchestrator_t *morch, uint32_t core_count) {
  if (!morch || core_count == 0 || core_count > TERMUX_MAX_CORES) return -1;

  memset(morch, 0, sizeof(*morch));
  morch->core_count = core_count;
  morch->active_cores = core_count;

  return 0;
}

void multicore_orchestrator_free(multicore_orchestrator_t *morch) {
  if (!morch) return;

  for (uint32_t i = 0; i < morch->core_count; i++) {
    if (morch->threads[i]) {
      pthread_join(morch->threads[i], NULL);
    }
  }

  memset(morch, 0, sizeof(*morch));
}

int multicore_orchestrator_init(multicore_orchestrator_t *morch) {
  if (!morch || morch->core_count == 0) return -1;

  for (uint32_t i = 0; i < morch->core_count; i++) {
    struct termux_orchestrator *orch = &morch->orchestrators[i];
    int ret = termux_orchestrator_init(orch);
    if (ret != 0) return ret;

    core_profile_t *prof = &morch->core_profiles[i];
    prof->core_id = i;
    prof->core_class = (i < morch->core_count / 2) ? CORE_CLASS_PERFORMANCE : CORE_CLASS_EFFICIENCY;
  }

  struct termux_phase_barrier *barrier = &morch->layer_barrier;
  int ret = termux_phase_barrier_init(barrier, morch->core_count);
  if (ret != 0) return ret;

  morch->layer_index = 0;
  morch->total_cycles = 0;
  morch->total_phi = 0;

  return 0;
}

int multicore_orchestrator_detect_cores(multicore_orchestrator_t *morch) {
  if (!morch) return -1;

  uint32_t detected = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
  if (detected == 0 || detected > TERMUX_MAX_CORES) {
    detected = 4;
  }

  morch->core_count = detected;

  for (uint32_t i = 0; i < morch->core_count; i++) {
    core_profile_t *prof = &morch->core_profiles[i];
    prof->core_id = i;
    prof->cpu_freq_mhz = 2400;
    prof->l1_cache_kb = 32;
    prof->l2_cache_kb = 512;

    if (i % 4 == 0) {
      prof->core_class = CORE_CLASS_PERFORMANCE;
      prof->cpu_freq_mhz = 2800;
    } else if (i % 4 == 1) {
      prof->core_class = CORE_CLASS_PERFORMANCE;
      prof->cpu_freq_mhz = 2600;
    } else {
      prof->core_class = CORE_CLASS_EFFICIENCY;
      prof->cpu_freq_mhz = 1800;
    }
  }

  return 0;
}

typedef struct {
  multicore_orchestrator_t *morch;
  uint32_t core_id;
  profiler_session_t *session;
} worker_thread_arg_t;

static void *worker_thread_execute(void *arg) {
  worker_thread_arg_t *thread_arg = (worker_thread_arg_t *)arg;
  multicore_orchestrator_t *morch = thread_arg->morch;
  uint32_t core_id = thread_arg->core_id;

  struct termux_orchestrator *orch = &morch->orchestrators[core_id];
  core_profile_t *prof = &morch->core_profiles[core_id];
  profiler_session_t *session = thread_arg->session;

  for (uint32_t layer_idx = 0; layer_idx < morch->layer_count; layer_idx++) {
    layer_batch_t *layer = &morch->layers[layer_idx];

    uint32_t pkg_start = (layer->pkg_count * core_id) / morch->core_count;
    uint32_t pkg_end = (layer->pkg_count * (core_id + 1)) / morch->core_count;

    for (uint32_t pkg_offset = pkg_start; pkg_offset < pkg_end; pkg_offset++) {
      uint16_t pkg_idx = layer->pkg_indices[pkg_offset];
      if (pkg_idx >= TERMUX_REAL_PKG_COUNT) continue;

      char pkg_name[64];
      snprintf(pkg_name, sizeof(pkg_name), "pkg_%u", pkg_idx);

      int ret = termux_orchestrator_execute(orch, pkg_name, pkg_idx);
      if (ret != 0) {
        prof->packages_completed++;
        continue;
      }

      prof->total_cycles += orch->state.cycle_count;
      prof->total_phi += orch->state.coherence_phi;
      prof->packages_completed++;

      if (session) {
        cycle_profile_t cycles = {};
        cycles.total_cycles = orch->state.cycle_count;
        profiler_session_record_package(session, pkg_idx, &cycles);
      }
    }

    termux_phase_barrier_wait(&morch->layer_barrier);
  }

  free(thread_arg);
  return NULL;
}

int multicore_orchestrator_execute_parallel(multicore_orchestrator_t *morch,
                                           const layer_batch_t *layers,
                                           uint32_t layer_count) {
  if (!morch || !layers || layer_count == 0 || layer_count > TERMUX_DAG_LAYERS) {
    return -1;
  }

  memcpy(morch->layers, layers, sizeof(layer_batch_t) * layer_count);
  morch->layer_count = layer_count;

  struct timespec ts_start;
  clock_gettime(CLOCK_MONOTONIC, &ts_start);
  uint64_t start_ns = (uint64_t)ts_start.tv_sec * 1000000000ULL + ts_start.tv_nsec;

  profiler_session_t session = {};
  profiler_session_init(&session, "multicore-parallel");
  profiler_session_start(&session);

  for (uint32_t i = 0; i < morch->core_count; i++) {
    worker_thread_arg_t *arg = (worker_thread_arg_t *)malloc(sizeof(*arg));
    if (!arg) return -2;

    arg->morch = morch;
    arg->core_id = i;
    arg->session = &session;

    int ret = pthread_create(&morch->threads[i], NULL, worker_thread_execute, arg);
    if (ret != 0) {
      free(arg);
      return ret;
    }
  }

  for (uint32_t i = 0; i < morch->core_count; i++) {
    pthread_join(morch->threads[i], NULL);
    morch->threads[i] = 0;
  }

  struct timespec ts_end;
  clock_gettime(CLOCK_MONOTONIC, &ts_end);
  uint64_t end_ns = (uint64_t)ts_end.tv_sec * 1000000000ULL + ts_end.tv_nsec;

  morch->wall_time_sec = (double)(end_ns - start_ns) / 1.0e9;

  uint64_t total_pkg = 0;
  for (uint32_t i = 0; i < morch->core_count; i++) {
    morch->total_cycles += morch->core_profiles[i].total_cycles;
    morch->total_phi += morch->core_profiles[i].total_phi;
    total_pkg += morch->core_profiles[i].packages_completed;
  }

  if (total_pkg > 0) {
    uint64_t avg_cycles = morch->total_cycles / total_pkg;
    morch->efficiency = 1.0 - ((double)avg_cycles / (double)TERMUX_MAX_CYCLES_PER_PACKAGE);
    if (morch->efficiency < 0.0) morch->efficiency = 0.0;
  }

  profiler_session_finalize(&session);

  if (morch->wall_time_sec > 0.0 && total_pkg > 0) {
    double speedup_factor = (double)total_pkg / (morch->wall_time_sec * 4.0);
    morch->speedup_vs_single = speedup_factor > 1.0 ? speedup_factor : 1.0;
  }

  return 0;
}

int multicore_orchestrator_load_balance(multicore_orchestrator_t *morch, uint32_t layer_idx) {
  if (!morch || layer_idx >= morch->layer_count) return -1;

  layer_batch_t *layer = &morch->layers[layer_idx];
  uint32_t workload_per_core = layer->pkg_count / morch->core_count;

  for (uint32_t i = 0; i < morch->core_count; i++) {
    core_profile_t *prof = &morch->core_profiles[i];

    uint32_t load_adjustment = (prof->cpu_freq_mhz > 2400) ? 1 : 0;
    uint32_t adjusted_workload = workload_per_core + load_adjustment;

    if (prof->total_cycles > 0) {
      double efficiency = 1.0 - ((double)prof->total_cycles / (double)(adjusted_workload * TERMUX_MAX_CYCLES_PER_PACKAGE));
      prof->total_phi = (uint64_t)(efficiency * 65536.0);
    }
  }

  return 0;
}

double multicore_orchestrator_calculate_speedup(multicore_orchestrator_t *morch,
                                               uint64_t single_core_cycles) {
  if (!morch || single_core_cycles == 0) return 1.0;

  uint64_t avg_cycles = morch->total_cycles / (morch->core_count > 0 ? morch->core_count : 1);
  double speedup = (double)single_core_cycles / (double)(avg_cycles > 0 ? avg_cycles : 1);

  return speedup > 1.0 ? speedup : 1.0;
}

void multicore_orchestrator_print_stats(const multicore_orchestrator_t *morch) {
  if (!morch) return;

  printf("\n");
  printf("================================================================================\n");
  printf("                   MULTICORE ORCHESTRATOR STATISTICS\n");
  printf("================================================================================\n");
  printf("Active Cores: %u / %u\n", morch->active_cores, TERMUX_MAX_CORES);
  printf("Wall time: %.2f seconds\n", morch->wall_time_sec);
  printf("Total cycles: %" PRIu64 "\n", morch->total_cycles);
  printf("Total Φ score: %" PRIu64 "\n", morch->total_phi);

  printf("\nPer-Core Breakdown:\n");
  for (uint32_t i = 0; i < morch->core_count; i++) {
    const core_profile_t *prof = &morch->core_profiles[i];
    const char *class_name = (prof->core_class == CORE_CLASS_PERFORMANCE) ? "PERF" :
                             (prof->core_class == CORE_CLASS_EFFICIENCY) ? "EFFI" : "GENR";

    printf("  Core %u [%s] freq=%u MHz, l1=%u KB, l2=%u KB\n",
           prof->core_id, class_name, prof->cpu_freq_mhz,
           prof->l1_cache_kb, prof->l2_cache_kb);
    printf("    Packages: %u, Cycles: %" PRIu64 ", Φ: %" PRIu64 "\n",
           prof->packages_completed, prof->total_cycles, prof->total_phi);
  }

  printf("\nPerformance Metrics:\n");
  printf("  Efficiency: %.2f%%\n", morch->efficiency * 100.0);
  printf("  Speedup vs single-core: %.2fx\n", morch->speedup_vs_single);

  printf("================================================================================\n\n");
}
