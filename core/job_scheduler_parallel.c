#include "job_scheduler_parallel.h"
#include "phase_barrier_lockfree.h"
#include "cpu_affinity.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct job_context {
  struct termux_job_scheduler *scheduler;
  uint32_t thread_id;
  uint32_t cpu_id;
  struct termux_build_state state;
  uint64_t total_cycles;
  uint64_t total_phi;
  volatile uint32_t layer_num;
};

static void* job_thread_worker(void *arg) {
  struct job_context *ctx = (struct job_context *)arg;
  struct termux_job_scheduler *sched = ctx->scheduler;

  termux_cpu_affinity_set(ctx->cpu_id);

  ctx->total_cycles = 0;
  ctx->total_phi = 0;

  for (uint32_t layer = 0; layer < TERMUX_DAG_LAYERS; layer++) {
    struct termux_layer_info *layer_info = &sched->layers[layer];
    uint32_t pkg_per_thread = (layer_info->package_count + sched->num_threads - 1) / sched->num_threads;
    uint32_t pkg_start = ctx->thread_id * pkg_per_thread;
    uint32_t pkg_end = pkg_start + pkg_per_thread;
    if (pkg_end > layer_info->package_count) {
      pkg_end = layer_info->package_count;
    }

    for (uint32_t pkg_idx = pkg_start; pkg_idx < pkg_end; pkg_idx++) {
      uint32_t pkg_id = layer_info->package_indices[pkg_idx];

      if (sched->execute_fn) {
        struct termux_build_state local_state = {};
        int ret = sched->execute_fn(pkg_id, &local_state);

        if (ret == 0) {
          ctx->total_cycles += local_state.cycle_count;
          ctx->total_phi += local_state.coherence_phi;
        }
      }
    }

    int barrier_ret = termux_phase_barrier_wait(&sched->barrier);
    if (barrier_ret != 0) {
      return NULL;
    }
  }

  sched->results[ctx->thread_id].total_cycles = ctx->total_cycles;
  sched->results[ctx->thread_id].total_phi = ctx->total_phi;
  sched->results[ctx->thread_id].thread_id = ctx->thread_id;

  free(ctx);
  return NULL;
}

int termux_job_scheduler_init(struct termux_job_scheduler *sched,
                              uint32_t num_threads,
                              const struct termux_layer_info *layers,
                              uint32_t layer_count) {
  if (!sched || num_threads == 0 || num_threads > TERMUX_MAX_THREADS) {
    return -1;
  }

  if (!layers || layer_count != TERMUX_DAG_LAYERS) {
    return -1;
  }

  memset(sched, 0, sizeof(*sched));
  sched->num_threads = num_threads;
  sched->layer_count = layer_count;
  sched->layers_per_thread = (layer_count + num_threads - 1) / num_threads;

  memcpy(sched->layers, layers, sizeof(struct termux_layer_info) * layer_count);

  if (termux_phase_barrier_init(&sched->barrier, num_threads) != 0) {
    return -1;
  }

  return 0;
}

int termux_job_scheduler_run(struct termux_job_scheduler *sched,
                             termux_execute_fn execute_fn) {
  if (!sched || !execute_fn) {
    return -1;
  }

  sched->execute_fn = execute_fn;

  pthread_t threads[TERMUX_MAX_THREADS];
  uint32_t cpu_count = termux_cpu_count();

  for (uint32_t i = 0; i < sched->num_threads; i++) {
    struct job_context *ctx = malloc(sizeof(*ctx));
    if (!ctx) {
      return -1;
    }

    ctx->scheduler = sched;
    ctx->thread_id = i;
    ctx->cpu_id = i % cpu_count;

    int ret = pthread_create(&threads[i], NULL, job_thread_worker, ctx);
    if (ret != 0) {
      free(ctx);
      return -1;
    }
  }

  for (uint32_t i = 0; i < sched->num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}

uint64_t termux_job_scheduler_total_cycles(struct termux_job_scheduler *sched) {
  if (!sched) {
    return 0;
  }

  uint64_t total = 0;
  for (uint32_t i = 0; i < sched->num_threads; i++) {
    total += sched->results[i].total_cycles;
  }
  return total;
}

uint64_t termux_job_scheduler_total_phi(struct termux_job_scheduler *sched) {
  if (!sched) {
    return 0;
  }

  uint64_t total = 0;
  for (uint32_t i = 0; i < sched->num_threads; i++) {
    total += sched->results[i].total_phi;
  }
  return total;
}

double termux_job_scheduler_mean_phi(struct termux_job_scheduler *sched) {
  if (!sched) {
    return 0.0;
  }

  uint64_t total_phi = termux_job_scheduler_total_phi(sched);
  uint32_t package_count = 0;

  for (uint32_t i = 0; i < sched->layer_count; i++) {
    package_count += sched->layers[i].package_count;
  }

  if (package_count == 0) {
    return 0.0;
  }

  return (double)total_phi / (double)package_count / (double)(1ULL << 16);
}

void termux_job_scheduler_reset(struct termux_job_scheduler *sched) {
  if (!sched) {
    return;
  }

  memset(sched->results, 0, sizeof(sched->results));
  termux_phase_barrier_reset(&sched->barrier);
}
