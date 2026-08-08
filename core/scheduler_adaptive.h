#ifndef SCHEDULER_ADAPTIVE_H
#define SCHEDULER_ADAPTIVE_H

/*
 * Phase 9.19: Adaptive Scheduling System
 * Priority queue based on coherence φ scores
 * High-Φ packages execute first (critical path prioritization)
 * Dynamic adjustment: +20% priority if Φ < 0.80
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ============================================================================
 * Package Priority & Scheduling
 * ============================================================================ */

typedef struct {
  uint32_t pkg_idx;          /* Package index (0..2056) */
  uint64_t coherence_phi;    /* Q48.16 coherence metric */
  uint32_t layer;            /* Toroidal layer (0..41) */
  uint8_t is_critical;       /* Critical path flag */
  uint32_t dynamic_priority; /* Adjusted priority (0..1000) */
  uint64_t submit_time;      /* When added to queue */
  uint64_t start_time;       /* When execution started */
  uint64_t completion_time;  /* When execution completed */
} scheduled_task_t;

typedef struct {
  uint32_t capacity;
  uint32_t count;
  scheduled_task_t *tasks;   /* Min-heap array */
  uint64_t current_phi;      /* Global coherence φ */
  uint64_t phi_threshold;    /* Threshold for dynamic adjustment (0.80) */
  uint32_t adjustments_made; /* Counter for adaptive triggers */
} priority_queue_t;

/* Initialize adaptive scheduler */
int scheduler_init(priority_queue_t *sched, uint32_t capacity);

/* Add task to priority queue */
int scheduler_enqueue(priority_queue_t *sched, const scheduled_task_t *task);

/* Get highest-priority task */
int scheduler_dequeue(priority_queue_t *sched, scheduled_task_t *out_task);

/* Update coherence metric and trigger dynamic adjustment if needed */
void scheduler_update_coherence(priority_queue_t *sched, uint64_t new_phi);

/* Compute dynamic priority with adaptive adjustment */
uint32_t scheduler_compute_priority(const scheduled_task_t *task, uint64_t current_phi);

/* Get queue statistics */
int scheduler_get_stats(const priority_queue_t *sched,
                        uint32_t *pending, uint32_t *total_adjustments);

/* Free scheduler resources */
void scheduler_free(priority_queue_t *sched);

/* ============================================================================
 * Layer-Wise Scheduling (Toroidal Topology)
 * ============================================================================ */

typedef struct {
  uint32_t layer_id;         /* Layer 0..41 */
  uint32_t pkg_count;        /* Packages in this layer */
  uint32_t *pkg_indices;     /* Package indices */
  uint64_t *phi_scores;      /* Coherence scores */
  uint64_t layer_mean_phi;   /* Mean φ for layer */
  uint8_t is_saturated;      /* All packages in layer completed */
} layer_schedule_t;

/* Create layer schedule */
int scheduler_layer_create(layer_schedule_t *layer, uint32_t layer_id,
                           uint32_t capacity);

/* Sort packages in layer by φ (descending) */
int scheduler_layer_sort_by_phi(layer_schedule_t *layer);

/* Free layer schedule */
void scheduler_layer_free(layer_schedule_t *layer);

/* ============================================================================
 * Benchmark & Profiling
 * ============================================================================ */

typedef struct {
  uint32_t total_tasks;
  uint32_t completed_tasks;
  double mean_wait_time_us;
  double max_wait_time_us;
  double mean_execution_time_us;
  uint32_t priority_adjustments;
  double mean_phi;
  double min_phi;
} scheduler_stats_t;

/* Compute scheduler statistics */
int scheduler_compute_stats(const priority_queue_t *sched,
                            scheduler_stats_t *stats);

/* Report scheduler performance */
void scheduler_report(FILE *out, const scheduler_stats_t *stats);

#endif  /* SCHEDULER_ADAPTIVE_H */
