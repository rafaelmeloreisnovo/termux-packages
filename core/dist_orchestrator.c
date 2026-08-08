#include "dist_orchestrator.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* Forward declaration */
static uint64_t get_current_time_ms(void);

/* ============================================================================
 * Master Orchestrator Implementation
 * ============================================================================ */

int dist_master_init(dist_master_t *master, uint32_t total_packages) {
  if (!master) return -1;

  memset(master, 0, sizeof(*master));
  master->total_packages = total_packages;
  master->master_start_time_ms = get_current_time_ms();

  for (uint32_t i = 0; i < MAX_LAYERS; i++) {
    master->layers[i].layer_id = i;
    master->layers[i].pkg_count = (total_packages + MAX_LAYERS - 1) / MAX_LAYERS;
    master->layers[i].assigned_worker = UINT32_MAX;
    master->layers[i].is_completed = 0;
    master->layers[i].coherence_phi = 0;
  }

  return 0;
}

int dist_master_register_worker(dist_master_t *master, const char *hostname,
                                uint16_t port, uint32_t *worker_id) {
  if (!master || !hostname || !worker_id) return -1;
  if (master->worker_count >= MAX_WORKERS) return -1;

  uint32_t id = master->worker_count;
  worker_node_t *worker = &master->workers[id];

  worker->worker_id = id;
  strncpy(worker->hostname, hostname, sizeof(worker->hostname) - 1);
  worker->port = port;
  worker->state = WORKER_IDLE;
  worker->current_pkg_idx = 0;
  worker->current_layer = 0;
  worker->mean_latency_us = 0;
  worker->builds_completed = 0;
  worker->builds_failed = 0;
  worker->last_heartbeat_ms = get_current_time_ms();
  worker->load_factor = 0.0;

  master->worker_count++;
  *worker_id = id;
  return 0;
}

int dist_master_assign_layer(dist_master_t *master, uint32_t layer_id,
                             uint32_t worker_id) {
  if (!master || layer_id >= MAX_LAYERS || worker_id >= master->worker_count)
    return -1;

  layer_assignment_t *layer = &master->layers[layer_id];
  worker_node_t *worker = &master->workers[worker_id];

  layer->assigned_worker = worker_id;
  layer->is_assigned = 1;
  layer->assign_time_ms = get_current_time_ms();

  worker->current_layer = layer_id;
  worker->state = WORKER_BUILDING;

  return 0;
}

int dist_master_update_worker(dist_master_t *master, uint32_t worker_id,
                              worker_state_t state, uint32_t completed_count) {
  if (!master || worker_id >= master->worker_count) return -1;

  worker_node_t *worker = &master->workers[worker_id];
  worker->state = state;
  worker->builds_completed = completed_count;
  worker->last_heartbeat_ms = get_current_time_ms();

  return 0;
}

int dist_master_check_health(dist_master_t *master) {
  if (!master) return -1;

  uint64_t now = get_current_time_ms();
  int failed_count = 0;

  for (uint32_t i = 0; i < master->worker_count; i++) {
    worker_node_t *worker = &master->workers[i];
    uint64_t elapsed = now - worker->last_heartbeat_ms;

    if (elapsed > WORKER_TIMEOUT_MS && worker->state != WORKER_FAILED) {
      worker->state = WORKER_FAILED;
      failed_count++;
    }
  }

  return failed_count;
}

int dist_master_get_next_layer(dist_master_t *master, uint32_t *layer_id) {
  if (!master || !layer_id) return -1;

  for (uint32_t i = 0; i < MAX_LAYERS; i++) {
    if (!master->layers[i].is_assigned) {
      *layer_id = i;
      return 0;
    }
  }

  return -1;
}

uint64_t dist_master_coherence_phi(dist_master_t *master) {
  if (!master) return 0;

  uint64_t sum_phi = 0;
  uint32_t completed = 0;

  for (uint32_t i = 0; i < MAX_LAYERS; i++) {
    if (master->layers[i].is_completed) {
      sum_phi += master->layers[i].coherence_phi;
      completed++;
    }
  }

  if (completed == 0) return 0;
  return sum_phi / completed;
}

void dist_master_free(dist_master_t *master) {
  if (master) {
    memset(master, 0, sizeof(*master));
  }
}

/* ============================================================================
 * Worker Node Implementation
 * ============================================================================ */

int dist_worker_init(dist_worker_t *worker, uint32_t worker_id,
                     const char *master_hostname, uint16_t master_port) {
  if (!worker || !master_hostname) return -1;

  memset(worker, 0, sizeof(*worker));
  worker->worker_id = worker_id;
  strncpy(worker->master_hostname, master_hostname,
          sizeof(worker->master_hostname) - 1);
  worker->master_port = master_port;
  worker->start_time_ms = get_current_time_ms();

  return 0;
}

int dist_worker_connect_master(dist_worker_t *worker) {
  if (!worker) return -1;

  worker->current_layer = 0;
  worker->current_pkg_idx = 0;

  return 0;
}

int dist_worker_get_assignment(dist_worker_t *worker, uint32_t *layer_id,
                               uint32_t **pkg_indices, uint32_t *pkg_count) {
  if (!worker || !layer_id || !pkg_indices || !pkg_count) return -1;

  *layer_id = worker->current_layer;
  *pkg_indices = &worker->current_pkg_idx;
  *pkg_count = 0;

  return 0;
}

int dist_worker_report_completion(dist_worker_t *worker, uint32_t layer_id,
                                  uint32_t packages_built) {
  if (!worker) return -1;

  worker->builds_completed += packages_built;
  worker->current_layer = layer_id;

  return 0;
}

int dist_worker_send_heartbeat(dist_worker_t *worker) {
  if (!worker) return -1;

  return 0;
}

void dist_worker_free(dist_worker_t *worker) {
  if (worker) {
    memset(worker, 0, sizeof(*worker));
  }
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int dist_get_stats(dist_master_t *master, dist_stats_t *stats) {
  if (!master || !stats) return -1;

  memset(stats, 0, sizeof(*stats));
  stats->total_workers = master->worker_count;
  stats->total_packages = master->total_packages;
  stats->total_layers = MAX_LAYERS;

  uint32_t active = 0;
  for (uint32_t i = 0; i < master->worker_count; i++) {
    if (master->workers[i].state != WORKER_FAILED &&
        master->workers[i].state != WORKER_DISCONNECTED) {
      active++;
    }
    stats->completed_packages += master->workers[i].builds_completed;
  }

  stats->active_workers = active;
  stats->failed_workers = master->worker_count - active;

  for (uint32_t i = 0; i < MAX_LAYERS; i++) {
    if (master->layers[i].is_completed) {
      stats->completed_layers++;
    }
  }

  stats->estimated_speedup = (active > 0) ? (double)active : 1.0;
  stats->elapsed_time_ms = get_current_time_ms() - master->master_start_time_ms;

  return 0;
}

void dist_report(FILE *out, const dist_stats_t *stats) {
  if (!out || !stats) return;

  fprintf(out, "=== Distributed Build Statistics ===\n");
  fprintf(out, "Total workers: %u\n", stats->total_workers);
  fprintf(out, "Active workers: %u\n", stats->active_workers);
  fprintf(out, "Failed workers: %u\n", stats->failed_workers);
  fprintf(out, "Total layers: %u\n", stats->total_layers);
  fprintf(out, "Completed layers: %u\n", stats->completed_layers);
  fprintf(out, "Total packages: %u\n", stats->total_packages);
  fprintf(out, "Completed packages: %u\n", stats->completed_packages);
  fprintf(out, "Estimated speedup: %.2f×\n", stats->estimated_speedup);
  fprintf(out, "Elapsed time: %" PRIu64 " ms\n", stats->elapsed_time_ms);
}

/* ============================================================================
 * Utility: Get current time in milliseconds
 * ============================================================================ */

static uint64_t get_current_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
