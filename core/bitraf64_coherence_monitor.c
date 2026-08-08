#include "bitraf64_coherence_monitor.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/*
 * BITRAF64 Coherence Monitoring Implementation (Phase 2)
 *
 * Real-time tracking and adaptive response to coherence degradation
 */

int bitraf64_coherence_monitor_init(
    bitraf64_coherence_monitor_t *monitor,
    uint32_t degradation_threshold_q48_16
) {
  if (!monitor) return 0;

  memset(monitor, 0, sizeof(*monitor));

  monitor->phi_degrade_threshold = degradation_threshold_q48_16;
  monitor->enabled = 1;

  return 1;
}

int bitraf64_coherence_monitor_record_package(
    bitraf64_coherence_monitor_t *monitor,
    const bitraf64_monitor_pkg_t *pkg_event
) {
  if (!monitor || !pkg_event || !monitor->enabled) return 0;

  uint32_t layer = pkg_event->layer;

  if (layer >= 32) return 0;  /* Invalid layer */

  /* Update layer aggregate stats */
  bitraf64_monitor_layer_t *layer_stats = &monitor->layers[layer];

  if (layer_stats->pkg_count == 0) {
    /* First package in this layer */
    layer_stats->layer = layer;
    layer_stats->mean_phi = pkg_event->coherence_phi;
    layer_stats->min_phi = pkg_event->coherence_phi;
    layer_stats->max_phi = pkg_event->coherence_phi;
  } else {
    /* Update aggregates */
    uint64_t total_phi = (layer_stats->mean_phi * layer_stats->pkg_count) +
                         pkg_event->coherence_phi;
    layer_stats->pkg_count++;
    layer_stats->mean_phi = total_phi / layer_stats->pkg_count;

    if (pkg_event->coherence_phi < layer_stats->min_phi) {
      layer_stats->min_phi = pkg_event->coherence_phi;
    }
    if (pkg_event->coherence_phi > layer_stats->max_phi) {
      layer_stats->max_phi = pkg_event->coherence_phi;
    }
  }

  /* Track slowest package */
  if (pkg_event->wall_time_ms > layer_stats->slowest_pkg_ms) {
    layer_stats->slowest_pkg_ms = pkg_event->wall_time_ms;
  }

  layer_stats->total_time_ms += pkg_event->wall_time_ms;
  layer_stats->timestamp_ns = pkg_event->timestamp_ns;

  return 1;
}

int bitraf64_coherence_monitor_record_layer(
    bitraf64_coherence_monitor_t *monitor,
    uint32_t layer,
    uint32_t pkg_count,
    const uint64_t *phi_scores
) {
  if (!monitor || !phi_scores || layer >= 32) return 0;

  bitraf64_monitor_layer_t *layer_stats = &monitor->layers[layer];

  layer_stats->layer = layer;
  layer_stats->pkg_count = pkg_count;

  /* Compute mean from phi_scores array */
  uint64_t sum = 0;
  uint64_t min_phi = UINT64_MAX;
  uint64_t max_phi = 0;

  for (uint32_t i = 0; i < pkg_count; i++) {
    sum += phi_scores[i];
    if (phi_scores[i] < min_phi) min_phi = phi_scores[i];
    if (phi_scores[i] > max_phi) max_phi = phi_scores[i];
  }

  layer_stats->mean_phi = sum / (uint64_t)pkg_count;
  layer_stats->min_phi = min_phi;
  layer_stats->max_phi = max_phi;
  layer_stats->timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;

  /* Check for anomaly */
  if (layer_stats->mean_phi < monitor->phi_degrade_threshold) {
    layer_stats->anomaly_detected = 1;
  }

  return 1;
}

int bitraf64_coherence_monitor_check_anomaly(
    const bitraf64_coherence_monitor_t *monitor,
    uint32_t layer
) {
  if (!monitor || layer >= 32) return 0;

  const bitraf64_monitor_layer_t *layer_stats = &monitor->layers[layer];

  return layer_stats->anomaly_detected;
}

float bitraf64_coherence_monitor_suggest_dvfs(
    const bitraf64_coherence_monitor_t *monitor,
    uint32_t layer
) {
  if (!monitor || layer >= 32) return 1.0f;

  const bitraf64_monitor_layer_t *layer_stats = &monitor->layers[layer];

  /* Convert Q48.16 to float for comparison */
  float phi_float = (float)layer_stats->mean_phi / 65536.0f;

  /* DVFS decision thresholds */
  float high_threshold = 0.95f;  /* Scale up if coherence excellent */
  float low_threshold = 0.75f;   /* Scale down if coherence poor */

  /* Suggest frequency adjustment */
  if (phi_float > high_threshold) {
    return 1.1f;  /* Increase frequency by 10% */
  } else if (phi_float < low_threshold) {
    return 0.9f;  /* Decrease frequency by 10% */
  } else {
    return 1.0f;  /* Maintain current frequency */
  }
}

int bitraf64_coherence_monitor_get_system_state(
    const bitraf64_coherence_monitor_t *monitor,
    bitraf64_monitor_system_t *out_state
) {
  if (!monitor || !out_state) return 0;

  memcpy(out_state, &monitor->system, sizeof(*out_state));

  return 1;
}

int bitraf64_coherence_monitor_get_layer_history(
    const bitraf64_coherence_monitor_t *monitor,
    bitraf64_monitor_layer_t *out_history,
    size_t history_size
) {
  if (!monitor || !out_history) return 0;

  size_t copy_size = (history_size < 32) ? history_size : 32;
  memcpy(out_history, monitor->layers, copy_size * sizeof(bitraf64_monitor_layer_t));

  return (int)copy_size;
}

int bitraf64_coherence_monitor_report(
    const bitraf64_coherence_monitor_t *monitor,
    char *buffer,
    size_t buffer_size
) {
  if (!monitor || !buffer || buffer_size < 256) return 0;

  int offset = 0;

  /* Header */
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "=== BITRAF64 Coherence Report ===\n\n");

  /* System-wide stats */
  float system_phi = (float)monitor->system.system_mean_phi / 65536.0f;
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "System Φ: %.4f (target > 0.85)\n",
                     system_phi);
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Total Time: %u ms\n\n",
                     monitor->system.total_time_ms);

  /* Per-layer summary */
  offset += snprintf(buffer + offset, buffer_size - offset,
                     "Layer Summary (top anomalies):\n");

  for (uint32_t layer = 0; layer < 32; layer++) {
    const bitraf64_monitor_layer_t *l = &monitor->layers[layer];

    if (l->pkg_count == 0) continue;  /* Unused layer */

    float phi = (float)l->mean_phi / 65536.0f;

    if (l->anomaly_detected) {
      offset += snprintf(buffer + offset, buffer_size - offset,
                         "  Layer %2u: Φ=%.4f ⚠️  (%u pkg, %u ms)\n",
                         layer, phi, l->pkg_count, l->total_time_ms);
    }
  }

  return offset;
}

int bitraf64_coherence_monitor_reset(
    bitraf64_coherence_monitor_t *monitor
) {
  if (!monitor) return 0;

  memset(monitor->layers, 0, sizeof(monitor->layers));
  memset(&monitor->system, 0, sizeof(monitor->system));

  monitor->enabled = 1;

  return 1;
}
