#ifndef TERMUX_BITRAF64_COHERENCE_MONITOR_H
#define TERMUX_BITRAF64_COHERENCE_MONITOR_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/*
 * BITRAF64 Coherence Monitoring (Phase 2)
 *
 * Real-time tracking of Φ (coherence) metric during build execution:
 *   1. Per-layer aggregation: Σ φ_pkg / pkg_count per layer
 *   2. Anomaly detection: φ degradation > threshold
 *   3. Adaptive DVFS triggers: frequency scaling based on φ trajectory
 *   4. Performance correlation: latency vs coherence tracking
 *
 * Used for:
 *   - Live coherence visualization on device
 *   - Adaptive power management (frequency scaling)
 *   - Build phase optimization (identify bottlenecks)
 *   - Predictive triggers (slow layer ahead)
 */

/* Coherence monitoring granularity */
typedef enum {
  BITRAF64_MONITOR_PACKAGE = 0,  /* Per-package tracking */
  BITRAF64_MONITOR_LAYER = 1,    /* Per-layer aggregation */
  BITRAF64_MONITOR_SYSTEM = 2    /* System-wide aggregate */
} bitraf64_monitor_level_t;

/* Per-package coherence snapshot */
typedef struct {
  uint32_t pkg_idx;               /* Package index */
  uint64_t coherence_phi;         /* Q48.16 fixed-point */
  uint32_t wall_time_ms;          /* Build time for this package (ms) */
  uint32_t l1_miss_rate;          /* L1 cache miss percentage (0-100) */
  uint32_t layer;                 /* Toroidal layer */
  uint64_t timestamp_ns;          /* Wallclock timestamp */
} bitraf64_monitor_pkg_t;

/* Per-layer coherence aggregate */
typedef struct {
  uint32_t layer;                 /* Layer number 0-31 */
  uint32_t pkg_count;             /* Packages in this layer */
  uint64_t mean_phi;              /* Mean coherence (Q48.16) */
  uint64_t min_phi;               /* Minimum coherence in layer */
  uint64_t max_phi;               /* Maximum coherence in layer */
  uint32_t total_time_ms;         /* Total time for layer (ms) */
  uint32_t slowest_pkg_ms;        /* Slowest package in layer (ms) */
  uint64_t timestamp_ns;          /* Layer completion time */
  uint8_t anomaly_detected;       /* 1 if φ degradation detected */
} bitraf64_monitor_layer_t;

/* System-wide coherence state */
typedef struct {
  uint32_t total_packages;        /* 2057 for termux */
  uint64_t system_mean_phi;       /* Overall mean coherence (Q48.16) */
  uint64_t system_min_phi;        /* Minimum coherence any package */
  uint64_t system_max_phi;        /* Maximum coherence any package */
  uint32_t total_time_ms;         /* Total build time (ms) */
  uint32_t phase_count;           /* Number of build phases (7) */
  uint8_t dvfs_active;            /* 1 if DVFS currently adjusting frequency */
  uint64_t timestamp_ns;          /* System completion time */
} bitraf64_monitor_system_t;

/* Coherence monitoring context (ring buffer for layers) */
typedef struct {
  /* Per-layer history (32 entries for 32 layers) */
  bitraf64_monitor_layer_t layers[32];

  /* System state */
  bitraf64_monitor_system_t system;

  /* Configuration */
  uint32_t phi_degrade_threshold;  /* Trigger threshold for anomaly (e.g., 0.75) */
  uint32_t enabled;                /* 1 = monitoring active */
  uint32_t _padding;
} bitraf64_coherence_monitor_t;

/*
 * Initialize coherence monitoring context
 *
 * Allocates and initializes ring buffer for per-layer tracking
 * Must be called once before any monitoring operations
 */
int bitraf64_coherence_monitor_init(
    bitraf64_coherence_monitor_t *monitor,
    uint32_t degradation_threshold_q48_16
);

/*
 * Record package completion event
 *
 * Called immediately after package build completes
 * Updates per-layer aggregate and checks for anomalies
 */
int bitraf64_coherence_monitor_record_package(
    bitraf64_coherence_monitor_t *monitor,
    const bitraf64_monitor_pkg_t *pkg_event
);

/*
 * Record layer completion event
 *
 * Called after all packages in a layer complete
 * Computes layer-wide statistics and triggers adaptive responses
 */
int bitraf64_coherence_monitor_record_layer(
    bitraf64_coherence_monitor_t *monitor,
    uint32_t layer,
    uint32_t pkg_count,
    const uint64_t *phi_scores
);

/*
 * Check for coherence anomalies
 *
 * Detects if current layer coherence is significantly worse than
 * previous layers (degradation > threshold)
 *
 * Returns: 1 if anomaly detected, 0 otherwise
 */
int bitraf64_coherence_monitor_check_anomaly(
    const bitraf64_coherence_monitor_t *monitor,
    uint32_t layer
);

/*
 * Suggest adaptive DVFS adjustment
 *
 * Based on coherence trajectory, recommends frequency scaling:
 *   - Increase frequency if φ > high_threshold (good coherence, can go faster)
 *   - Decrease frequency if φ < low_threshold (poor coherence, save power)
 *   - Maintain if within band
 *
 * Returns: suggested frequency scaling factor (0.8-1.2)
 */
float bitraf64_coherence_monitor_suggest_dvfs(
    const bitraf64_coherence_monitor_t *monitor,
    uint32_t layer
);

/*
 * Get current system coherence state
 */
int bitraf64_coherence_monitor_get_system_state(
    const bitraf64_coherence_monitor_t *monitor,
    bitraf64_monitor_system_t *out_state
);

/*
 * Get per-layer coherence history
 *
 * Returns snapshot of layer history for visualization/logging
 */
int bitraf64_coherence_monitor_get_layer_history(
    const bitraf64_coherence_monitor_t *monitor,
    bitraf64_monitor_layer_t *out_history,
    size_t history_size
);

/*
 * Format coherence report for logging
 *
 * Generates human-readable report of coherence progression
 * for console output or log file
 */
int bitraf64_coherence_monitor_report(
    const bitraf64_coherence_monitor_t *monitor,
    char *buffer,
    size_t buffer_size
);

/*
 * Reset monitoring state (for restart/resume scenarios)
 */
int bitraf64_coherence_monitor_reset(
    bitraf64_coherence_monitor_t *monitor
);

#endif /* TERMUX_BITRAF64_COHERENCE_MONITOR_H */
