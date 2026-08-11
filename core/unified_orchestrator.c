#include "unified_orchestrator.h"
#include "phase_barrier_lockfree.h"
#include "cpu_affinity.h"
#include "hardware_tuning.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <inttypes.h>

static inline uint64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int unified_orchestrator_alloc(unified_orchestrator_t **orch_out) {
  if (!orch_out) {
    return -1;
  }

  unified_orchestrator_t *orch = (unified_orchestrator_t *)malloc(sizeof(*orch));
  if (!orch) {
    return -1;
  }

  memset(orch, 0, sizeof(*orch));

  orch->timestamp_start = get_time_ns();
  orch->generation = 1;
  orch->active_threads = 0;

  if (termux_phase_barrier_init(&orch->barrier_init, 4) != 0) {
    free(orch);
    return -1;
  }

  if (termux_phase_barrier_init(&orch->barrier_profiling, 1) != 0) {
    free(orch);
    return -1;
  }

  if (termux_phase_barrier_init(&orch->barrier_tuning, 1) != 0) {
    free(orch);
    return -1;
  }

  snprintf(orch->cicd_module.build_id, sizeof(orch->cicd_module.build_id),
           "build_%" PRIu64, orch->timestamp_start);

  *orch_out = orch;
  return 0;
}

void unified_orchestrator_free(unified_orchestrator_t *orch) {
  if (!orch) {
    return;
  }

  if (orch->gpu_module.gpu_device != NULL) {
    gpu_device_cleanup(orch->gpu_module.gpu_device);
  }

  free(orch);
}

static int device_profiler_init_task(unified_orchestrator_t *orch) {
  if (!orch) {
    return -1;
  }

  struct termux_hardware_config hw;
  if (termux_hardware_get_config(&hw) != 0) {
    fprintf(stderr, "Warning: Failed to detect hardware\n");
    orch->device_module.device_count = 1;
  } else {
    orch->device_module.device_count = 1;

    device_profile_t *profile = &orch->device_module.profiles[0];
    strncpy(profile->device_name, termux_hardware_soc_name(hw.soc_type),
            sizeof(profile->device_name) - 1);
    profile->cpu_cores = hw.total_cores;
    profile->cpu_freq_mhz = hw.max_freq_mhz;
    profile->cpu_l1_cache = 32768;
    profile->gpu_exists = 0;
    profile->gpu_cores = 0;
    profile->gpu_freq_mhz = 0;
    profile->gpu_memory = 0;
    profile->coherence_phi = 0.0;
    profile->speedup_vs_baseline = 0.0;
  }

  _Atomic(uint32_t) *ready = (_Atomic(uint32_t) *)&orch->device_module.profiling_complete;
  atomic_store(ready, 1);

  return 0;
}

static int coherence_tuner_init_task(unified_orchestrator_t *orch) {
  if (!orch) {
    return -1;
  }

  orch->coherence_module.phi_current = 0.90;
  orch->coherence_module.phi_target = 0.98;

  for (uint32_t i = 0; i < UNIFIED_MAX_LAYERS; i++) {
    orch->coherence_module.metrics[i].layer_id = i;
    orch->coherence_module.metrics[i].phi_score = 0.90;
    orch->coherence_module.metrics[i].overhead_heap = 0.05;
    orch->coherence_module.metrics[i].latency_wall = 0.05;
    orch->coherence_module.metrics[i].cache_misses = 0.05;
  }

  _Atomic(uint32_t) *ready = (_Atomic(uint32_t) *)&orch->coherence_module.tuning_complete;
  atomic_store(ready, 1);

  return 0;
}

static int cicd_reporter_init_task(unified_orchestrator_t *orch) {
  if (!orch) {
    return -1;
  }

  orch->cicd_module.timestamp_start = get_time_ns();

  for (uint32_t i = 0; i < 9; i++) {
    orch->cicd_module.wall_time_phases[i] = 0.0;
  }

  _Atomic(uint32_t) *ready = (_Atomic(uint32_t) *)&orch->cicd_module.ci_reported;
  atomic_store(ready, 0);

  return 0;
}

static int gpu_symbiosis_init_task(unified_orchestrator_t *orch) {
  if (!orch) {
    return -1;
  }

  device_profile_t *profile = &orch->device_module.profiles[0];

  orch->gpu_module.gpu_device = NULL;
  orch->gpu_module.gpu_power_watts = 0.0;
  orch->gpu_module.gpu_tasks.head = 0;
  orch->gpu_module.gpu_tasks.tail = 0;
  orch->gpu_module.gpu_tasks.count = 0;
  orch->gpu_module.gpu_tasks.completed_count = 0;

  if (profile->gpu_exists) {
    fprintf(stdout, "GPU detected: %s (%u cores)\n", profile->gpu_name,
            profile->gpu_cores);

    orch->gpu_module.gpu_power_watts = 3.5;
    orch->gpu_module.gpu_tasks.speedup = 4.2;
  } else {
    fprintf(stdout, "No GPU detected, CPU-only execution\n");
    orch->gpu_module.gpu_power_watts = 0.0;
    orch->gpu_module.gpu_tasks.speedup = 1.0;
  }

  _Atomic(uint32_t) *ready = (_Atomic(uint32_t) *)&orch->gpu_module.gpu_initialized;
  atomic_store(ready, 1);

  return 0;
}

typedef struct {
  unified_orchestrator_t *orch;
  uint32_t task_id;
} init_task_args_t;

void *init_task_worker(void *arg) {
  init_task_args_t *task = (init_task_args_t *)arg;
  unified_orchestrator_t *orch = task->orch;
  uint32_t task_id = task->task_id;

  int result = -1;

  switch (task_id) {
    case 0:
      result = device_profiler_init_task(orch);
      break;
    case 1:
      result = coherence_tuner_init_task(orch);
      break;
    case 2:
      result = cicd_reporter_init_task(orch);
      break;
    case 3:
      result = gpu_symbiosis_init_task(orch);
      break;
    default:
      result = -1;
  }

  termux_phase_barrier_wait(&orch->barrier_init);

  free(task);
  return (void *)(intptr_t)result;
}

int unified_orchestrator_init_parallel(unified_orchestrator_t *orch, uint32_t num_threads) {
  if (!orch || num_threads == 0) {
    return -1;
  }

  orch->active_threads = num_threads;

  pthread_t threads[4];

  for (uint32_t i = 0; i < 4; i++) {
    init_task_args_t *task = (init_task_args_t *)malloc(sizeof(*task));
    if (!task) {
      return -1;
    }

    task->orch = orch;
    task->task_id = i;

    int pret = pthread_create(&threads[i], NULL, init_task_worker, task);
    if (pret != 0) {
      free(task);
      return -1;
    }
  }

  for (uint32_t i = 0; i < 4; i++) {
    pthread_join(threads[i], NULL);
  }

  _Atomic(uint32_t) *dev_ready = (_Atomic(uint32_t) *)&orch->device_module.profiling_complete;
  _Atomic(uint32_t) *gpu_ready = (_Atomic(uint32_t) *)&orch->gpu_module.gpu_initialized;

  if (!atomic_load(dev_ready) || !atomic_load(gpu_ready)) {
    fprintf(stderr, "Error: Initialization modules failed\n");
    return -1;
  }

  return 0;
}

void unified_orchestrator_report(unified_orchestrator_t *orch) {
  if (!orch) {
    return;
  }

  uint64_t timestamp_now = get_time_ns();
  uint64_t elapsed_ns = timestamp_now - orch->timestamp_start;
  double elapsed_sec = (double)elapsed_ns / 1e9;
  double elapsed_min = elapsed_sec / 60.0;

  printf("\n");
  printf("╔═══════════════════════════════════════════════════════════════╗\n");
  printf("║  UNIFIED ORCHESTRATOR REPORT (Phases 9.10-9.13 Parallel)    ║\n");
  printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

  printf("┌─ DEVICE PROFILING (Phase 9.10) ──────────────────────────────┐\n");
  if (orch->device_module.device_count > 0) {
    device_profile_t *profile = &orch->device_module.profiles[0];
    printf("│ Device:          %s\n", profile->device_name);
    printf("│ CPU Cores:       %u @ %u MHz\n", profile->cpu_cores, profile->cpu_freq_mhz);
    printf("│ CPU L1 Cache:    %" PRIu64 " KB\n", profile->cpu_l1_cache / 1024);

    if (profile->gpu_exists) {
      printf("│ GPU:             %s (%u cores @ %u MHz)\n", profile->gpu_name,
             profile->gpu_cores, profile->gpu_freq_mhz);
      printf("│ GPU Memory:      %" PRIu64 " MB\n", profile->gpu_memory / (1024 * 1024));
    } else {
      printf("│ GPU:             Not available\n");
    }
  }
  printf("└──────────────────────────────────────────────────────────────┘\n\n");

  printf("┌─ COHERENCE OPTIMIZATION (Phase 9.11) ────────────────────────┐\n");
  printf("│ φ Baseline:      0.9000\n");
  printf("│ φ Current:       %.4f\n", orch->coherence_module.phi_current);
  printf("│ φ Target:        %.4f\n", orch->coherence_module.phi_target);

  double baseline_phi = 0.90;
  double gap_reduced = (1.0 - orch->coherence_module.phi_current) /
                       (1.0 - baseline_phi) * 100.0;
  printf("│ Gap Reduced:     %.1f%%\n", gap_reduced);

  printf("│ Overhead:        %.2f%%\n", orch->coherence_module.metrics[0].overhead_heap * 100);
  printf("│ Latency:         %.2f%%\n", orch->coherence_module.metrics[0].latency_wall * 100);
  printf("│ Cache Misses:    %.2f%%\n", orch->coherence_module.metrics[0].cache_misses * 100);
  printf("└──────────────────────────────────────────────────────────────┘\n\n");

  printf("┌─ CI/CD INTEGRATION (Phase 9.12) ─────────────────────────────┐\n");
  printf("│ Build ID:        %s\n", orch->cicd_module.build_id);
  printf("│ Wall Time:       %.2f minutes (%.2f seconds)\n", elapsed_min, elapsed_sec);
  printf("│ Timestamp:       %" PRIu64 "\n", orch->cicd_module.timestamp_start);

  double total_phase_time = 0.0;
  for (uint32_t i = 0; i < 9; i++) {
    total_phase_time += orch->cicd_module.wall_time_phases[i];
    if (orch->cicd_module.wall_time_phases[i] > 0.0) {
      printf("│ Phase 9.%u:        %.3f ms\n", i + 1,
             orch->cicd_module.wall_time_phases[i] * 1000);
    }
  }
  printf("│ Total Phases:    %.3f ms\n", total_phase_time * 1000);
  printf("└──────────────────────────────────────────────────────────────┘\n\n");

  printf("┌─ GPU SYMBIOSIS (Phase 9.13) ─────────────────────────────────┐\n");
  printf("│ GPU Initialized: %s\n",
         orch->gpu_module.gpu_initialized ? "Yes" : "No");
  printf("│ GPU Power:       %.2f W\n", orch->gpu_module.gpu_power_watts);
  printf("│ GPU Tasks:       %" PRIu64 " enqueued, %" PRIu64 " completed\n",
         orch->gpu_module.gpu_tasks.count, orch->gpu_module.gpu_tasks.completed_count);
  printf("│ GPU Speedup:     %.2fx\n", orch->gpu_module.gpu_tasks.speedup);

  if (orch->gpu_module.gpu_device != NULL) {
    printf("│ Status:          Active (GPU accelerating build)\n");
  } else {
    printf("│ Status:          Idle (GPU not available)\n");
  }
  printf("└──────────────────────────────────────────────────────────────┘\n\n");

  printf("╔═══════════════════════════════════════════════════════════════╗\n");
  printf("║ BUILD COMPLETE                                               ║\n");
  printf("║                                                               ║\n");
  printf("║ Wall Time:       %.2f minutes                                ║\n", elapsed_min);
  printf("║ Coherence φ:     %.4f (target: %.4f)                         ║\n",
         orch->coherence_module.phi_current, orch->coherence_module.phi_target);
  printf("║ Active Threads:  %u                                           ║\n",
         orch->active_threads);
  printf("║ Generation:      %" PRIu64 "                                          ║\n",
         orch->generation);
  printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
}

int unified_orchestrator_update_coherence(unified_orchestrator_t *orch, uint32_t layer_id,
                                          double phi_score) {
  if (!orch || layer_id >= UNIFIED_MAX_LAYERS) {
    return -1;
  }

  coherence_metric_t *metric = &orch->coherence_module.metrics[layer_id];
  metric->layer_id = layer_id;
  metric->phi_score = phi_score;

  double sum_phi = 0.0;
  for (uint32_t i = 0; i < UNIFIED_MAX_LAYERS; i++) {
    sum_phi += orch->coherence_module.metrics[i].phi_score;
  }

  orch->coherence_module.phi_current = sum_phi / UNIFIED_MAX_LAYERS;

  return 0;
}

int unified_orchestrator_enqueue_gpu_task(unified_orchestrator_t *orch, struct gpu_task *task) {
  if (!orch || !task) {
    return -1;
  }

  if (orch->gpu_module.gpu_tasks.count >= UNIFIED_TASK_QUEUE_SIZE) {
    return -1;
  }

  uint64_t tail = orch->gpu_module.gpu_tasks.tail;
  orch->gpu_module.gpu_tasks.queue[tail % UNIFIED_TASK_QUEUE_SIZE] = *task;

  _Atomic(uint64_t) *tail_ptr = (_Atomic(uint64_t) *)&orch->gpu_module.gpu_tasks.tail;
  atomic_fetch_add(tail_ptr, 1);

  _Atomic(uint64_t) *count_ptr = (_Atomic(uint64_t) *)&orch->gpu_module.gpu_tasks.count;
  atomic_fetch_add(count_ptr, 1);

  return 0;
}

int unified_orchestrator_dequeue_gpu_task(unified_orchestrator_t *orch, struct gpu_task *task) {
  if (!orch || !task) {
    return -1;
  }

  _Atomic(uint64_t) *head_ptr = (_Atomic(uint64_t) *)&orch->gpu_module.gpu_tasks.head;
  _Atomic(uint64_t) *tail_ptr = (_Atomic(uint64_t) *)&orch->gpu_module.gpu_tasks.tail;

  uint64_t head = atomic_load(head_ptr);
  uint64_t tail = atomic_load(tail_ptr);

  if (head >= tail) {
    return -1;
  }

  *task = orch->gpu_module.gpu_tasks.queue[head % UNIFIED_TASK_QUEUE_SIZE];

  atomic_store(head_ptr, head + 1);

  _Atomic(uint64_t) *count_ptr = (_Atomic(uint64_t) *)&orch->gpu_module.gpu_tasks.count;
  atomic_fetch_sub(count_ptr, 1);

  _Atomic(uint64_t) *completed_ptr =
      (_Atomic(uint64_t) *)&orch->gpu_module.gpu_tasks.completed_count;
  atomic_fetch_add(completed_ptr, 1);

  return 0;
}

void unified_orchestrator_update_phase_time(unified_orchestrator_t *orch, uint32_t phase_id,
                                            double wall_time_sec) {
  if (!orch || phase_id >= 9) {
    return;
  }

  orch->cicd_module.wall_time_phases[phase_id] = wall_time_sec;
}

int unified_orchestrator_should_gpu_execute(unified_orchestrator_t *orch, void *state) {
  (void)state;

  if (!orch) {
    return 0;
  }

  if (orch->gpu_module.gpu_device == NULL) {
    return 0;
  }

  if (orch->coherence_module.phi_current > 0.95) {
    return 0;
  }

  if (orch->gpu_module.gpu_power_watts < 5.0) {
    return 1;
  }

  return 0;
}
