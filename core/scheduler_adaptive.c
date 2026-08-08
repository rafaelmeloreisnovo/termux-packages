#include "scheduler_adaptive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Min-Heap Priority Queue (Higher φ = Higher Priority)
 * ============================================================================ */

static int compare_priority(const scheduled_task_t *a, const scheduled_task_t *b) {
  /* Higher dynamic_priority first (reverse order for min-heap of priorities) */
  if (a->dynamic_priority > b->dynamic_priority) return -1;
  if (a->dynamic_priority < b->dynamic_priority) return 1;
  return 0;
}

static void heap_swap(scheduled_task_t *a, scheduled_task_t *b) {
  scheduled_task_t temp = *a;
  *a = *b;
  *b = temp;
}

static void heap_sift_up(priority_queue_t *q, uint32_t idx) {
  while (idx > 0) {
    uint32_t parent = (idx - 1) / 2;
    if (compare_priority(&q->tasks[idx], &q->tasks[parent]) >= 0) break;
    heap_swap(&q->tasks[idx], &q->tasks[parent]);
    idx = parent;
  }
}

static void heap_sift_down(priority_queue_t *q, uint32_t idx) {
  while (idx * 2 + 1 < q->count) {
    uint32_t left = idx * 2 + 1;
    uint32_t right = idx * 2 + 2;
    uint32_t smallest = idx;

    if (left < q->count && compare_priority(&q->tasks[left], &q->tasks[smallest]) < 0) {
      smallest = left;
    }
    if (right < q->count && compare_priority(&q->tasks[right], &q->tasks[smallest]) < 0) {
      smallest = right;
    }

    if (smallest == idx) break;
    heap_swap(&q->tasks[idx], &q->tasks[smallest]);
    idx = smallest;
  }
}

/* ============================================================================
 * Scheduler Implementation
 * ============================================================================ */

int scheduler_init(priority_queue_t *sched, uint32_t capacity) {
  if (!sched || capacity == 0) return -1;

  sched->tasks = (scheduled_task_t *)malloc(capacity * sizeof(scheduled_task_t));
  if (!sched->tasks) return -2;

  sched->capacity = capacity;
  sched->count = 0;
  sched->current_phi = (uint64_t)(0.85 * (1ULL << 16));  /* Q48.16: 0.85 */
  sched->phi_threshold = (uint64_t)(0.80 * (1ULL << 16)); /* Q48.16: 0.80 */
  sched->adjustments_made = 0;

  return 0;
}

uint32_t scheduler_compute_priority(const scheduled_task_t *task, uint64_t current_phi) {
  if (!task) return 0;

  /* Base priority: scale φ to 0-1000 range */
  uint32_t base_priority = (uint32_t)((task->coherence_phi >> 16) * 1000);
  if (base_priority > 1000) base_priority = 1000;

  /* Critical path packages get +100 priority boost */
  if (task->is_critical) {
    base_priority = (base_priority * 120) / 100;  /* +20% */
  }

  /* Dynamic adjustment: if Φ < 0.80, increase priority by 20% */
  if (current_phi < (uint64_t)(0.80 * (1ULL << 16))) {
    if (task->coherence_phi < current_phi) {
      /* Low-φ tasks get extra priority when system struggling */
      base_priority = (base_priority * 120) / 100;
    }
  }

  return base_priority > 1000 ? 1000 : base_priority;
}

int scheduler_enqueue(priority_queue_t *sched, const scheduled_task_t *task) {
  if (!sched || !task) return -1;

  if (sched->count >= sched->capacity) {
    return -2;  /* Queue full */
  }

  /* Add task and compute its dynamic priority */
  scheduled_task_t new_task = *task;
  new_task.dynamic_priority = scheduler_compute_priority(task, sched->current_phi);
  new_task.submit_time = (uint64_t)time(NULL) * 1000000;  /* microseconds */

  sched->tasks[sched->count] = new_task;
  heap_sift_up(sched, sched->count);
  sched->count++;

  return 0;
}

int scheduler_dequeue(priority_queue_t *sched, scheduled_task_t *out_task) {
  if (!sched || !out_task) return -1;

  if (sched->count == 0) {
    return -2;  /* Queue empty */
  }

  /* Extract minimum (highest priority) */
  *out_task = sched->tasks[0];
  out_task->start_time = (uint64_t)time(NULL) * 1000000;

  /* Move last to front and sift down */
  sched->tasks[0] = sched->tasks[sched->count - 1];
  sched->count--;
  if (sched->count > 0) {
    heap_sift_down(sched, 0);
  }

  return 0;
}

void scheduler_update_coherence(priority_queue_t *sched, uint64_t new_phi) {
  if (!sched) return;

  uint64_t old_phi = sched->current_phi;
  sched->current_phi = new_phi;

  /* If Φ dropped below threshold, increment adjustment counter */
  if (new_phi < sched->phi_threshold && old_phi >= sched->phi_threshold) {
    sched->adjustments_made++;
  }

  /* Recompute priorities for all queued tasks */
  for (uint32_t i = 0; i < sched->count; i++) {
    sched->tasks[i].dynamic_priority = scheduler_compute_priority(&sched->tasks[i], new_phi);
  }

  /* Rebuild heap */
  for (int32_t i = (int32_t)(sched->count / 2) - 1; i >= 0; i--) {
    heap_sift_down(sched, (uint32_t)i);
  }
}

int scheduler_get_stats(const priority_queue_t *sched,
                        uint32_t *pending, uint32_t *total_adjustments) {
  if (!sched) return -1;

  if (pending) *pending = sched->count;
  if (total_adjustments) *total_adjustments = sched->adjustments_made;

  return 0;
}

void scheduler_free(priority_queue_t *sched) {
  if (!sched) return;
  free(sched->tasks);
  memset(sched, 0, sizeof(*sched));
}

/* ============================================================================
 * Layer-Wise Scheduling
 * ============================================================================ */

int scheduler_layer_create(layer_schedule_t *layer, uint32_t layer_id,
                           uint32_t capacity) {
  if (!layer || capacity == 0) return -1;

  layer->layer_id = layer_id;
  layer->pkg_count = 0;
  layer->pkg_indices = (uint32_t *)malloc(capacity * sizeof(uint32_t));
  layer->phi_scores = (uint64_t *)malloc(capacity * sizeof(uint64_t));

  if (!layer->pkg_indices || !layer->phi_scores) {
    free(layer->pkg_indices);
    free(layer->phi_scores);
    return -2;
  }

  layer->layer_mean_phi = 0;
  layer->is_saturated = 0;

  return 0;
}

int scheduler_layer_sort_by_phi(layer_schedule_t *layer) {
  if (!layer || layer->pkg_count == 0) return -1;

  /* Simple bubble sort (sufficient for layer sizes ~49) */
  for (uint32_t i = 0; i < layer->pkg_count; i++) {
    for (uint32_t j = i + 1; j < layer->pkg_count; j++) {
      if (layer->phi_scores[j] > layer->phi_scores[i]) {
        /* Swap */
        uint64_t temp_phi = layer->phi_scores[i];
        layer->phi_scores[i] = layer->phi_scores[j];
        layer->phi_scores[j] = temp_phi;

        uint32_t temp_idx = layer->pkg_indices[i];
        layer->pkg_indices[i] = layer->pkg_indices[j];
        layer->pkg_indices[j] = temp_idx;
      }
    }
  }

  /* Compute mean Φ */
  uint64_t sum = 0;
  for (uint32_t i = 0; i < layer->pkg_count; i++) {
    sum += layer->phi_scores[i];
  }
  layer->layer_mean_phi = sum / layer->pkg_count;

  return 0;
}

void scheduler_layer_free(layer_schedule_t *layer) {
  if (!layer) return;
  free(layer->pkg_indices);
  free(layer->phi_scores);
  memset(layer, 0, sizeof(*layer));
}

/* ============================================================================
 * Statistics & Reporting
 * ============================================================================ */

int scheduler_compute_stats(const priority_queue_t *sched,
                            scheduler_stats_t *stats) {
  if (!sched || !stats) return -1;

  memset(stats, 0, sizeof(*stats));

  stats->total_tasks = sched->count;
  stats->priority_adjustments = sched->adjustments_made;
  stats->mean_phi = (double)sched->current_phi / (1ULL << 16);

  return 0;
}

void scheduler_report(FILE *out, const scheduler_stats_t *stats) {
  if (!out) out = stdout;
  if (!stats) return;

  fprintf(out, "=== Adaptive Scheduler Report ===\n");
  fprintf(out, "Total tasks processed: %u\n", stats->total_tasks);
  fprintf(out, "Completed: %u\n", stats->completed_tasks);
  fprintf(out, "Mean wait time: %.2f µs\n", stats->mean_wait_time_us);
  fprintf(out, "Max wait time: %.2f µs\n", stats->max_wait_time_us);
  fprintf(out, "Mean execution time: %.2f µs\n", stats->mean_execution_time_us);
  fprintf(out, "Priority adjustments: %u\n", stats->priority_adjustments);
  fprintf(out, "Mean coherence Φ: %.4f\n", stats->mean_phi);
  fprintf(out, "Min coherence Φ: %.4f\n", stats->min_phi);
  fprintf(out, "\n");
}
