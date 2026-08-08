#ifndef TERMUX_JOB_SCHEDULER_PARALLEL_H
#define TERMUX_JOB_SCHEDULER_PARALLEL_H

#include <stdint.h>
#include "build_orchestrator.h"
#include "phase_barrier_lockfree.h"

#define TERMUX_MAX_THREADS 16
#define TERMUX_MAX_PACKAGES_PER_LAYER 512

struct termux_layer_info {
  uint32_t layer_id;
  uint32_t package_indices[TERMUX_MAX_PACKAGES_PER_LAYER];
  uint32_t package_count;
  uint32_t toroidal_depth;
  uint64_t layer_phi;
};

struct termux_scheduler_result {
  uint32_t thread_id;
  uint64_t total_cycles;
  uint64_t total_phi;
};

typedef int (*termux_execute_fn)(uint32_t pkg_idx, struct termux_build_state *state);

struct termux_job_scheduler {
  uint32_t num_threads;
  uint32_t layer_count;
  uint32_t layers_per_thread;
  struct termux_layer_info layers[TERMUX_ORCHESTRATOR_TOTAL_STATES];
  struct termux_scheduler_result results[TERMUX_MAX_THREADS];
  struct termux_phase_barrier barrier;
  termux_execute_fn execute_fn;
  uint8_t _pad[64];
} __attribute__((aligned(256)));

int termux_job_scheduler_init(struct termux_job_scheduler *sched,
                              uint32_t num_threads,
                              const struct termux_layer_info *layers,
                              uint32_t layer_count);

int termux_job_scheduler_run(struct termux_job_scheduler *sched,
                             termux_execute_fn execute_fn);

uint64_t termux_job_scheduler_total_cycles(struct termux_job_scheduler *sched);

uint64_t termux_job_scheduler_total_phi(struct termux_job_scheduler *sched);

double termux_job_scheduler_mean_phi(struct termux_job_scheduler *sched);

void termux_job_scheduler_reset(struct termux_job_scheduler *sched);

#endif
