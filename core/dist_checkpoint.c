#include "dist_checkpoint.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* Forward declaration */
static uint64_t get_checkpoint_time_ms(void);

/* ============================================================================
 * Distributed Checkpoint Manager Implementation
 * ============================================================================ */

int dist_checkpoint_init(dist_checkpoint_mgr_t *mgr, uint32_t worker_id,
                         const char *nfs_mount_path, uint32_t total_layers) {
  if (!mgr || !nfs_mount_path) return -1;

  memset(mgr, 0, sizeof(*mgr));
  mgr->worker_id = worker_id;
  mgr->total_layers = total_layers;
  strncpy(mgr->nfs_mount_path, nfs_mount_path, sizeof(mgr->nfs_mount_path) - 1);

  mgr->layer_checkpoints =
      (checkpoint_meta_t *)malloc(total_layers * sizeof(checkpoint_meta_t));
  if (!mgr->layer_checkpoints) return -1;

  memset(mgr->layer_checkpoints, 0,
         total_layers * sizeof(checkpoint_meta_t));

  mgr->max_phi_observed = 0;
  mgr->conflict_count = 0;
  mgr->sync_count = 0;

  return 0;
}

int dist_checkpoint_write(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                          uint32_t pkg_idx, uint64_t coherence_phi,
                          const uint8_t *checkpoint_data, size_t data_size) {
  if (!mgr || layer_id >= mgr->total_layers) return -1;

  checkpoint_meta_t *meta = &mgr->layer_checkpoints[layer_id];
  meta->layer_id = layer_id;
  meta->pkg_idx = pkg_idx;
  meta->worker_id = mgr->worker_id;
  meta->coherence_phi = coherence_phi;
  meta->timestamp_ms = get_checkpoint_time_ms();
  meta->is_valid = 1;

  if (coherence_phi > mgr->max_phi_observed) {
    mgr->max_phi_observed = coherence_phi;
  }

  if (checkpoint_data && data_size > 0 && data_size <= 64) {
    dist_checkpoint_hash(checkpoint_data, data_size, meta->checkpoint_hash);
  }

  return 0;
}

int dist_checkpoint_read(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                         uint8_t *checkpoint_data, size_t *data_size) {
  if (!mgr || layer_id >= mgr->total_layers || !data_size) return -1;

  checkpoint_meta_t *meta = &mgr->layer_checkpoints[layer_id];
  if (!meta->is_valid) return -1;

  if (checkpoint_data && data_size) {
    *data_size = 0;
  }

  return 0;
}

int dist_checkpoint_sync(dist_checkpoint_mgr_t *mgr) {
  if (!mgr) return -1;

  mgr->sync_count++;
  return 0;
}

int dist_checkpoint_detect_conflicts(dist_checkpoint_mgr_t *mgr,
                                     uint32_t layer_id) {
  if (!mgr || layer_id >= mgr->total_layers) return -1;

  checkpoint_meta_t *meta = &mgr->layer_checkpoints[layer_id];
  if (!meta->is_valid) return 0;

  return 0;
}

int dist_checkpoint_resolve_conflict(dist_checkpoint_mgr_t *mgr,
                                     uint32_t layer_id) {
  if (!mgr || layer_id >= mgr->total_layers) return -1;

  checkpoint_meta_t *meta = &mgr->layer_checkpoints[layer_id];

  mgr->conflict_count++;

  if (meta->coherence_phi > mgr->max_phi_observed) {
    mgr->max_phi_observed = meta->coherence_phi;
  }

  return 0;
}

/* ============================================================================
 * Recovery & Reassignment
 * ============================================================================ */

int dist_checkpoint_handle_crash(dist_checkpoint_mgr_t *mgr,
                                 uint32_t failed_worker,
                                 uint32_t *next_layer_id,
                                 uint32_t *reassign_worker) {
  if (!mgr || !next_layer_id || !reassign_worker) return -1;

  for (uint32_t i = 0; i < mgr->total_layers; i++) {
    checkpoint_meta_t *meta = &mgr->layer_checkpoints[i];
    if (meta->worker_id == failed_worker && meta->is_valid) {
      *next_layer_id = i;
      *reassign_worker = (failed_worker + 1) % 4;
      return 0;
    }
  }

  return -1;
}

int dist_checkpoint_resume(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                           uint8_t *checkpoint_data, size_t *data_size) {
  if (!mgr || layer_id >= mgr->total_layers) return -1;

  return dist_checkpoint_read(mgr, layer_id, checkpoint_data, data_size);
}

/* ============================================================================
 * Checkpoint Validation
 * ============================================================================ */

int dist_checkpoint_verify(dist_checkpoint_mgr_t *mgr, uint32_t layer_id,
                           const uint8_t *checkpoint_data, size_t data_size) {
  if (!mgr || layer_id >= mgr->total_layers) return -1;

  checkpoint_meta_t *meta = &mgr->layer_checkpoints[layer_id];
  if (!meta->is_valid) return -1;

  char computed_hash[64];
  dist_checkpoint_hash(checkpoint_data, data_size, computed_hash);

  if (strncmp(meta->checkpoint_hash, computed_hash, 63) != 0) {
    return -1;
  }

  return 0;
}

void dist_checkpoint_hash(const uint8_t *data, size_t size, char *hex_hash) {
  if (!data || !hex_hash) return;

  memset(hex_hash, 0, 64);

  uint32_t hash = 0;
  for (size_t i = 0; i < size; i++) {
    hash ^= (uint32_t)data[i];
    hash = (hash << 1) | (hash >> 31);
  }

  snprintf(hex_hash, 64, "%08x", hash);
}

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

int dist_checkpoint_get_stats(dist_checkpoint_mgr_t *mgr,
                              checkpoint_stats_t *stats) {
  if (!mgr || !stats) return -1;

  memset(stats, 0, sizeof(*stats));

  uint32_t valid = 0;
  uint64_t sum_phi = 0;

  for (uint32_t i = 0; i < mgr->total_layers; i++) {
    checkpoint_meta_t *meta = &mgr->layer_checkpoints[i];
    if (meta->is_valid) {
      valid++;
      sum_phi += meta->coherence_phi;
      if (meta->coherence_phi > stats->max_phi) {
        stats->max_phi = (double)meta->coherence_phi / (1ULL << 16);
      }
    }
  }

  stats->total_checkpoints = mgr->total_layers;
  stats->valid_checkpoints = valid;
  stats->corrupted_checkpoints = 0;
  stats->conflicts_detected = mgr->conflict_count;
  stats->conflicts_resolved = mgr->conflict_count;
  stats->worker_recoveries = 0;

  if (valid > 0) {
    stats->mean_phi = (double)(sum_phi / valid) / (1ULL << 16);
  } else {
    stats->mean_phi = 0.0;
  }

  return 0;
}

void dist_checkpoint_report(FILE *out, const checkpoint_stats_t *stats) {
  if (!out || !stats) return;

  fprintf(out, "=== Checkpoint Statistics ===\n");
  fprintf(out, "Total checkpoints: %u\n", stats->total_checkpoints);
  fprintf(out, "Valid checkpoints: %u\n", stats->valid_checkpoints);
  fprintf(out, "Corrupted checkpoints: %u\n", stats->corrupted_checkpoints);
  fprintf(out, "Conflicts detected: %u\n", stats->conflicts_detected);
  fprintf(out, "Conflicts resolved: %u\n", stats->conflicts_resolved);
  fprintf(out, "Worker recoveries: %u\n", stats->worker_recoveries);
  fprintf(out, "Mean Φ: %.4f\n", stats->mean_phi);
  fprintf(out, "Max Φ: %.4f\n", stats->max_phi);
}

void dist_checkpoint_free(dist_checkpoint_mgr_t *mgr) {
  if (!mgr) return;

  if (mgr->layer_checkpoints) {
    free(mgr->layer_checkpoints);
    mgr->layer_checkpoints = NULL;
  }

  memset(mgr, 0, sizeof(*mgr));
}

/* ============================================================================
 * Utility: Get current time in milliseconds
 * ============================================================================ */

static uint64_t get_checkpoint_time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
