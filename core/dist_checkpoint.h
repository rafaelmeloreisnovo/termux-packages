#ifndef DIST_CHECKPOINT_H
#define DIST_CHECKPOINT_H

/*
 * Phase 9.20: Distributed Checkpoint Synchronization
 * Coordinate checkpoints across workers via shared NFS
 * Strategy: Write-through to NFS, max(Φ) conflict resolution
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* ============================================================================
 * Distributed Checkpoint Metadata
 * ============================================================================ */

typedef struct {
  uint32_t layer_id;
  uint32_t pkg_idx;
  uint32_t worker_id;
  uint64_t coherence_phi;
  uint64_t timestamp_ms;
  char checkpoint_hash[64];  /* SHA256 hex digest */
  uint8_t is_valid;
} checkpoint_meta_t;

/* ============================================================================
 * Distributed Checkpoint Manager
 * ============================================================================ */

typedef struct {
  char nfs_mount_path[256];  /* Shared NFS checkpoint location */
  uint32_t worker_id;
  uint32_t total_layers;
  checkpoint_meta_t *layer_checkpoints;
  uint64_t max_phi_observed;
  uint32_t conflict_count;
  uint32_t sync_count;
} dist_checkpoint_mgr_t;

/* Initialize distributed checkpoint manager */
int dist_checkpoint_init(dist_checkpoint_mgr_t *mgr, uint32_t worker_id,
                         const char *nfs_mount_path, uint32_t total_layers);

/* Write checkpoint to shared NFS */
int dist_checkpoint_write(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                          uint32_t pkg_idx, uint64_t coherence_phi,
                          const uint8_t *checkpoint_data, size_t data_size);

/* Read checkpoint from shared NFS */
int dist_checkpoint_read(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                         uint8_t *checkpoint_data, size_t *data_size);

/* Synchronize checkpoints across workers (resolve conflicts) */
int dist_checkpoint_sync(dist_checkpoint_mgr_t *mgr);

/* Detect conflicts (same layer, different φ scores) */
int dist_checkpoint_detect_conflicts(dist_checkpoint_mgr_t *mgr,
                                     uint32_t layer_id);

/* Resolve conflict: keep checkpoint with max(Φ) */
int dist_checkpoint_resolve_conflict(dist_checkpoint_mgr_t *mgr,
                                     uint32_t layer_id);

/* ============================================================================
 * Recovery & Reassignment
 * ============================================================================ */

typedef enum {
  RECOVERY_IDLE,
  RECOVERY_REASSIGN_LAYER,
  RECOVERY_RECOMPUTE_WORKER,
  RECOVERY_FAILED
} recovery_strategy_t;

/* Handle worker crash recovery */
int dist_checkpoint_handle_crash(dist_checkpoint_mgr_t *mgr, uint32_t failed_worker,
                                 uint32_t *next_layer_id, uint32_t *reassign_worker);

/* Resume from checkpoint after worker crash */
int dist_checkpoint_resume(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                           uint8_t *checkpoint_data, size_t *data_size);

/* ============================================================================
 * Checkpoint Validation
 * ============================================================================ */

/* Verify checkpoint integrity (SHA256) */
int dist_checkpoint_verify(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                           const uint8_t *checkpoint_data, size_t data_size);

/* Compute checkpoint SHA256 hash */
void dist_checkpoint_hash(const uint8_t *data, size_t size, char *hex_hash);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

typedef struct {
  uint32_t total_checkpoints;
  uint32_t valid_checkpoints;
  uint32_t corrupted_checkpoints;
  uint32_t conflicts_detected;
  uint32_t conflicts_resolved;
  uint32_t worker_recoveries;
  double mean_phi;
  double max_phi;
} checkpoint_stats_t;

/* Get checkpoint statistics */
int dist_checkpoint_get_stats(dist_checkpoint_mgr_t *mgr, checkpoint_stats_t *stats);

/* Report checkpoint status */
void dist_checkpoint_report(FILE *out, const checkpoint_stats_t *stats);

/* Free checkpoint manager */
void dist_checkpoint_free(dist_checkpoint_mgr_t *mgr);

#endif  /* DIST_CHECKPOINT_H */
