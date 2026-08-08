#ifndef DIST_ORCHESTRATOR_H
#define DIST_ORCHESTRATOR_H

/*
 * Phase 9.20: Distributed Build Orchestrator
 * Master coordinates with 4-8 worker nodes via RPC
 * Expected speedup: 3.2-3.5× (9.7 min → 4-5 min for 2057 packages)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>

/* ============================================================================
 * Worker Node Configuration
 * ============================================================================ */

#define MAX_WORKERS 8
#define MAX_LAYERS 42
#define HEARTBEAT_INTERVAL_MS 5000
#define WORKER_TIMEOUT_MS 30000

typedef enum {
  WORKER_IDLE,
  WORKER_BUILDING,
  WORKER_WAITING,
  WORKER_FAILED,
  WORKER_DISCONNECTED
} worker_state_t;

typedef struct {
  uint32_t worker_id;
  char hostname[64];
  uint16_t port;
  worker_state_t state;
  uint32_t current_pkg_idx;
  uint32_t current_layer;
  uint64_t mean_latency_us;
  uint32_t builds_completed;
  uint32_t builds_failed;
  uint64_t last_heartbeat_ms;
  double load_factor;  /* 0.0-1.0 */
} worker_node_t;

/* ============================================================================
 * Layer Assignment & Coordination
 * ============================================================================ */

typedef struct {
  uint32_t layer_id;
  uint32_t pkg_count;
  uint32_t *pkg_indices;
  uint8_t is_assigned;
  uint8_t is_completed;
  uint32_t assigned_worker;
  uint64_t assign_time_ms;
  uint64_t completion_time_ms;
  uint64_t coherence_phi;
} layer_assignment_t;

/* ============================================================================
 * Master Orchestrator State
 * ============================================================================ */

typedef struct {
  uint32_t worker_count;
  worker_node_t workers[MAX_WORKERS];
  layer_assignment_t layers[MAX_LAYERS];
  uint32_t total_packages;
  uint32_t packages_completed;
  uint32_t packages_failed;
  uint64_t master_start_time_ms;
  uint64_t coherence_phi_global;
  uint8_t is_shutdown;
} dist_master_t;

/* ============================================================================
 * Master Operations
 * ============================================================================ */

/* Initialize master orchestrator */
int dist_master_init(dist_master_t *master, uint32_t total_packages);

/* Register worker node */
int dist_master_register_worker(dist_master_t *master, const char *hostname,
                                uint16_t port, uint32_t *worker_id);

/* Assign layer to worker */
int dist_master_assign_layer(dist_master_t *master, uint32_t layer_id,
                             uint32_t worker_id);

/* Update worker status */
int dist_master_update_worker(dist_master_t *master, uint32_t worker_id,
                              worker_state_t state, uint32_t completed_count);

/* Check worker health (heartbeat) */
int dist_master_check_health(dist_master_t *master);

/* Get next layer to assign (load-balanced) */
int dist_master_get_next_layer(dist_master_t *master, uint32_t *layer_id);

/* Compute global coherence φ */
uint64_t dist_master_coherence_phi(dist_master_t *master);

/* Free orchestrator resources */
void dist_master_free(dist_master_t *master);

/* ============================================================================
 * Worker Node Operations
 * ============================================================================ */

typedef struct {
  uint32_t worker_id;
  char master_hostname[64];
  uint16_t master_port;
  char local_checkpoint_dir[256];
  uint32_t current_layer;
  uint32_t current_pkg_idx;
  uint32_t builds_completed;
  uint32_t builds_failed;
  uint64_t start_time_ms;
} dist_worker_t;

/* Initialize worker node */
int dist_worker_init(dist_worker_t *worker, uint32_t worker_id,
                     const char *master_hostname, uint16_t master_port);

/* Connect to master */
int dist_worker_connect_master(dist_worker_t *worker);

/* Get assigned layer from master */
int dist_worker_get_assignment(dist_worker_t *worker, uint32_t *layer_id,
                               uint32_t **pkg_indices, uint32_t *pkg_count);

/* Report build completion */
int dist_worker_report_completion(dist_worker_t *worker, uint32_t layer_id,
                                  uint32_t packages_built);

/* Send heartbeat to master */
int dist_worker_send_heartbeat(dist_worker_t *worker);

/* Free worker resources */
void dist_worker_free(dist_worker_t *worker);

/* ============================================================================
 * Statistics & Reporting
 * ============================================================================ */

typedef struct {
  uint32_t total_workers;
  uint32_t active_workers;
  uint32_t failed_workers;
  uint32_t total_layers;
  uint32_t completed_layers;
  uint32_t total_packages;
  uint32_t completed_packages;
  uint32_t failed_packages;
  double estimated_speedup;
  uint64_t elapsed_time_ms;
  uint64_t eta_ms;
} dist_stats_t;

/* Compute distributed build statistics */
int dist_get_stats(dist_master_t *master, dist_stats_t *stats);

/* Report distributed build progress */
void dist_report(FILE *out, const dist_stats_t *stats);

#endif  /* DIST_ORCHESTRATOR_H */
