#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

/*
 * Phase 9.20: Load Balancer
 * Dynamic layer assignment based on worker latency
 * Algorithm: assign next layer to worker with minimum mean latency
 * Rebalance: every 8 layers or if imbalance > 10%
 */

#include <stdint.h>

/* ============================================================================
 * Worker Load Metrics
 * ============================================================================ */

typedef struct {
  uint32_t worker_id;
  uint32_t layers_assigned;
  uint32_t layers_completed;
  uint64_t mean_latency_us;
  uint64_t min_latency_us;
  uint64_t max_latency_us;
  double load_factor;  /* 0.0-1.0: current load relative to capacity */
  uint64_t total_time_us;
} worker_load_t;

/* ============================================================================
 * Load Balancer State
 * ============================================================================ */

typedef struct {
  uint32_t worker_count;
  worker_load_t *workers;
  uint32_t total_layers_assigned;
  uint32_t rebalance_interval;  /* layers between rebalancing */
  double imbalance_threshold;   /* 0.10 = 10% */
  uint32_t last_rebalance_layer;
} load_balancer_t;

/* Initialize load balancer */
int lb_init(load_balancer_t *lb, uint32_t worker_count);

/* Update worker metrics */
int lb_update_worker(load_balancer_t *lb, uint32_t worker_id,
                     uint64_t latency_us, uint8_t layer_completed);

/* Get next worker to assign layer to (minimum latency) */
uint32_t lb_select_worker(load_balancer_t *lb);

/* Check if rebalancing needed */
uint8_t lb_needs_rebalance(load_balancer_t *lb);

/* Compute load imbalance (0.0-1.0) */
double lb_compute_imbalance(load_balancer_t *lb);

/* Compute average worker latency */
uint64_t lb_mean_latency(load_balancer_t *lb);

/* Rebalance worker assignments */
int lb_rebalance(load_balancer_t *lb);

/* Free load balancer */
void lb_free(load_balancer_t *lb);

#endif  /* LOAD_BALANCER_H */
