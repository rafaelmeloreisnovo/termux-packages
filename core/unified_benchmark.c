#include "unified_orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

typedef struct {
  unified_orchestrator_t *orch;
  uint32_t thread_id;
  uint32_t layers_per_thread;
} worker_context_t;

void *worker_thread(void *arg) {
  worker_context_t *ctx = (worker_context_t *)arg;
  unified_orchestrator_t *orch = ctx->orch;

  printf("Worker %u starting, assigned %u layers\n", ctx->thread_id, ctx->layers_per_thread);

  for (uint32_t layer = 0; layer < ctx->layers_per_thread; layer++) {
    uint32_t global_layer = ctx->thread_id * ctx->layers_per_thread + layer;

    if (global_layer >= 42) {
      break;
    }

    double phi_score = 0.90 + ((double)global_layer / 42.0) * 0.08;
    unified_orchestrator_update_coherence(orch, global_layer, phi_score);

    struct gpu_task task;
    task.type = GPU_TASK_PHI_COMPUTE;
    task.package_count = 49;
    task.profitability_score = (uint64_t)phi_score * 1000;

    if (unified_orchestrator_should_gpu_execute(orch, NULL)) {
      unified_orchestrator_enqueue_gpu_task(orch, &task);
    }

    unified_orchestrator_update_phase_time(orch, (layer % 9), 0.001);

    usleep(10000);
  }

  return NULL;
}

void *gpu_thread(void *arg) {
  unified_orchestrator_t *orch = (unified_orchestrator_t *)arg;

  printf("GPU thread starting\n");

  uint32_t tasks_processed = 0;

  for (uint32_t i = 0; i < 100; i++) {
    struct gpu_task task;

    if (unified_orchestrator_dequeue_gpu_task(orch, &task) == 0) {
      tasks_processed++;
      usleep(5000);
    } else {
      usleep(1000);
    }
  }

  printf("GPU thread completed: %u tasks processed\n", tasks_processed);

  return NULL;
}

void *monitor_thread(void *arg) {
  unified_orchestrator_t *orch = (unified_orchestrator_t *)arg;

  printf("Monitor thread starting\n");

  for (uint32_t i = 0; i < 50; i++) {
    printf("[Monitor] Current φ: %.4f | GPU Power: %.2fW | GPU Tasks: %" PRIu64 "/%" PRIu64 "\n",
           orch->coherence_module.phi_current, orch->gpu_module.gpu_power_watts,
           orch->gpu_module.gpu_tasks.completed_count, orch->gpu_module.gpu_tasks.count);

    usleep(200000);
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  uint32_t num_threads = 4;

  if (argc > 1) {
    num_threads = (uint32_t)atoi(argv[1]);
    if (num_threads == 0 || num_threads > 16) {
      num_threads = 4;
    }
  }

  printf("╔════════════════════════════════════════════════════════╗\n");
  printf("║   UNIFIED ORCHESTRATOR BENCHMARK (9.10-9.13 Parallel)  ║\n");
  printf("╚════════════════════════════════════════════════════════╝\n\n");

  unified_orchestrator_t *orch = NULL;

  if (unified_orchestrator_alloc(&orch) != 0) {
    fprintf(stderr, "Error: Failed to allocate orchestrator\n");
    return 1;
  }

  printf("Initializing modules in parallel (Phase 9.10-9.13)...\n");

  if (unified_orchestrator_init_parallel(orch, num_threads) != 0) {
    fprintf(stderr, "Error: Parallel initialization failed\n");
    unified_orchestrator_free(orch);
    return 1;
  }

  printf("✓ All modules initialized successfully\n\n");

  printf("Starting parallel execution with %u threads...\n\n", num_threads);

  pthread_t worker_threads[num_threads];
  pthread_t gpu_tid, monitor_tid;
  worker_context_t contexts[num_threads];

  uint32_t layers_per_thread = 42 / num_threads;

  for (uint32_t i = 0; i < num_threads; i++) {
    contexts[i].orch = orch;
    contexts[i].thread_id = i;
    contexts[i].layers_per_thread = layers_per_thread;

    int pret = pthread_create(&worker_threads[i], NULL, worker_thread, &contexts[i]);
    if (pret != 0) {
      fprintf(stderr, "Error: Failed to create worker thread %u\n", i);
      unified_orchestrator_free(orch);
      return 1;
    }
  }

  int pret = pthread_create(&gpu_tid, NULL, gpu_thread, orch);
  if (pret != 0) {
    fprintf(stderr, "Error: Failed to create GPU thread\n");
    unified_orchestrator_free(orch);
    return 1;
  }

  pret = pthread_create(&monitor_tid, NULL, monitor_thread, orch);
  if (pret != 0) {
    fprintf(stderr, "Error: Failed to create monitor thread\n");
    unified_orchestrator_free(orch);
    return 1;
  }

  for (uint32_t i = 0; i < num_threads; i++) {
    pthread_join(worker_threads[i], NULL);
  }
  pthread_join(gpu_tid, NULL);
  pthread_join(monitor_tid, NULL);

  printf("\n");

  unified_orchestrator_report(orch);

  unified_orchestrator_free(orch);

  printf("Benchmark completed successfully\n");

  return 0;
}
