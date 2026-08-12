#ifndef TERMUX_UNIFIED_ORCHESTRATOR_H
#define TERMUX_UNIFIED_ORCHESTRATOR_H

#include <stdint.h>
#include <stddef.h>
#include "phase_barrier_lockfree.h"
#include "build_orchestrator.h"
#include "gpu_device.h"

#define UNIFIED_MAX_DEVICES 16
#define UNIFIED_MAX_LAYERS 32
#define UNIFIED_TASK_QUEUE_SIZE 4096
#define UNIFIED_PHASE_COUNT 9

typedef struct {
  char device_name[256];
  uint32_t cpu_cores;
  uint32_t cpu_freq_mhz;
  uint64_t cpu_l1_cache;
  uint32_t gpu_exists;
  char gpu_name[256];
  uint32_t gpu_cores;
  uint32_t gpu_freq_mhz;
  uint64_t gpu_memory;
  double wall_time_orchestrator;
  double wall_time_simd;
  uint64_t l1_misses;
  uint64_t llc_misses;
  double power_cpu_watts;
  double power_gpu_watts;
  double coherence_phi;
  double speedup_vs_baseline;
} device_profile_t;

typedef struct {
  uint32_t layer_id;
  double phi_score;
  double overhead_heap;
  double latency_wall;
  double cache_misses;
} coherence_metric_t;

typedef enum {
  GPU_TASK_CRC32C_BATCH,
  GPU_TASK_SHA256_MANIFEST,
  GPU_TASK_PHI_COMPUTE,
  GPU_TASK_PATTERN_MATCH,
} gpu_task_type_t;

typedef struct gpu_task {
  gpu_task_type_t type;
  uint32_t package_count;
  void *input_buffer;
  void *output_buffer;
  uint64_t profitability_score;
} gpu_task_t;

typedef void gpu_device_t;

typedef struct {
  volatile uint64_t head;
  volatile uint64_t tail;
  volatile uint64_t count;
  volatile uint64_t completed_count;
  double speedup;
  gpu_task_t queue[UNIFIED_TASK_QUEUE_SIZE];
} gpu_task_queue_t;

typedef struct {
  volatile uint64_t device_count;
  device_profile_t profiles[UNIFIED_MAX_DEVICES];
  volatile uint32_t profiling_complete;
} device_module_t;

typedef struct {
  double phi_current;
  double phi_target;
  coherence_metric_t metrics[UNIFIED_MAX_LAYERS];
  volatile uint32_t tuning_complete;
} coherence_module_t;

typedef struct {
  char build_id[64];
  uint64_t timestamp_start;
  double wall_time_phases[UNIFIED_PHASE_COUNT];
  volatile uint32_t ci_reported;
  double overall_speedup;
} cicd_module_t;

typedef struct {
  gpu_device_t *gpu_device;
  gpu_task_queue_t gpu_tasks;
  volatile uint32_t gpu_initialized;
  double gpu_power_watts;
} gpu_module_t;

typedef struct {
  device_module_t device_module;
  coherence_module_t coherence_module;
  cicd_module_t cicd_module;
  gpu_module_t gpu_module;

  struct termux_phase_barrier barrier_init;
  struct termux_phase_barrier barrier_profiling;
  struct termux_phase_barrier barrier_tuning;

  volatile uint64_t generation;
  volatile uint32_t active_threads;
  uint64_t timestamp_start;
} unified_orchestrator_t;

int unified_orchestrator_alloc(unified_orchestrator_t **orch_out);

void unified_orchestrator_free(unified_orchestrator_t *orch);

int unified_orchestrator_init_parallel(unified_orchestrator_t *orch, uint32_t num_threads);

void unified_orchestrator_report(unified_orchestrator_t *orch);

int unified_orchestrator_update_coherence(unified_orchestrator_t *orch, uint32_t layer_id,
                                          double phi_score);

int unified_orchestrator_enqueue_gpu_task(unified_orchestrator_t *orch, struct gpu_task *task);

int unified_orchestrator_dequeue_gpu_task(unified_orchestrator_t *orch, struct gpu_task *task);

void unified_orchestrator_update_phase_time(unified_orchestrator_t *orch, uint32_t phase_id,
                                            double wall_time_sec);

int unified_orchestrator_should_gpu_execute(unified_orchestrator_t *orch, void *state);

#endif
