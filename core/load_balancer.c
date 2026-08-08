#include "load_balancer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Load Balancer Implementation
 * ============================================================================ */

int lb_init(load_balancer_t *lb, uint32_t worker_count) {
  if (!lb || worker_count == 0) return -1;

  memset(lb, 0, sizeof(*lb));
  lb->worker_count = worker_count;
  lb->rebalance_interval = 8;
  lb->imbalance_threshold = 0.10;
  lb->last_rebalance_layer = 0;

  lb->workers = (worker_load_t *)malloc(worker_count * sizeof(worker_load_t));
  if (!lb->workers) return -1;

  memset(lb->workers, 0, worker_count * sizeof(worker_load_t));

  for (uint32_t i = 0; i < worker_count; i++) {
    lb->workers[i].worker_id = i;
    lb->workers[i].load_factor = 0.0;
  }

  return 0;
}

int lb_update_worker(load_balancer_t *lb, uint32_t worker_id,
                     uint64_t latency_us, uint8_t layer_completed) {
  if (!lb || worker_id >= lb->worker_count) return -1;

  worker_load_t *worker = &lb->workers[worker_id];
  worker->mean_latency_us = latency_us;

  if (layer_completed) {
    worker->layers_completed++;
  }

  if (latency_us < worker->min_latency_us || worker->min_latency_us == 0) {
    worker->min_latency_us = latency_us;
  }

  if (latency_us > worker->max_latency_us) {
    worker->max_latency_us = latency_us;
  }

  worker->total_time_us += latency_us;

  return 0;
}

uint32_t lb_select_worker(load_balancer_t *lb) {
  if (!lb || lb->worker_count == 0) return 0;

  uint32_t best_worker = 0;
  uint64_t best_latency = lb->workers[0].mean_latency_us;

  for (uint32_t i = 1; i < lb->worker_count; i++) {
    if (lb->workers[i].mean_latency_us < best_latency) {
      best_worker = i;
      best_latency = lb->workers[i].mean_latency_us;
    }
  }

  return best_worker;
}

uint8_t lb_needs_rebalance(load_balancer_t *lb) {
  if (!lb) return 0;

  if (lb->total_layers_assigned - lb->last_rebalance_layer >=
      lb->rebalance_interval) {
    return 1;
  }

  double imbalance = lb_compute_imbalance(lb);
  if (imbalance > lb->imbalance_threshold) {
    return 1;
  }

  return 0;
}

double lb_compute_imbalance(load_balancer_t *lb) {
  if (!lb || lb->worker_count == 0) return 0.0;

  uint32_t min_layers = lb->workers[0].layers_assigned;
  uint32_t max_layers = lb->workers[0].layers_assigned;

  for (uint32_t i = 1; i < lb->worker_count; i++) {
    if (lb->workers[i].layers_assigned < min_layers) {
      min_layers = lb->workers[i].layers_assigned;
    }
    if (lb->workers[i].layers_assigned > max_layers) {
      max_layers = lb->workers[i].layers_assigned;
    }
  }

  if (max_layers == 0) return 0.0;

  return (double)(max_layers - min_layers) / (double)max_layers;
}

uint64_t lb_mean_latency(load_balancer_t *lb) {
  if (!lb || lb->worker_count == 0) return 0;

  uint64_t sum = 0;
  for (uint32_t i = 0; i < lb->worker_count; i++) {
    sum += lb->workers[i].mean_latency_us;
  }

  return sum / lb->worker_count;
}

int lb_rebalance(load_balancer_t *lb) {
  if (!lb) return -1;

  uint32_t min_layers = lb->workers[0].layers_assigned;
  uint32_t max_layers = lb->workers[0].layers_assigned;
  uint32_t min_worker = 0;
  uint32_t max_worker = 0;

  for (uint32_t i = 1; i < lb->worker_count; i++) {
    if (lb->workers[i].layers_assigned < min_layers) {
      min_layers = lb->workers[i].layers_assigned;
      min_worker = i;
    }
    if (lb->workers[i].layers_assigned > max_layers) {
      max_layers = lb->workers[i].layers_assigned;
      max_worker = i;
    }
  }

  if (max_layers - min_layers > 2) {
    lb->workers[max_worker].layers_assigned--;
    lb->workers[min_worker].layers_assigned++;
  }

  lb->last_rebalance_layer = lb->total_layers_assigned;

  return 0;
}

void lb_free(load_balancer_t *lb) {
  if (!lb) return;

  if (lb->workers) {
    free(lb->workers);
    lb->workers = NULL;
  }

  memset(lb, 0, sizeof(*lb));
}
