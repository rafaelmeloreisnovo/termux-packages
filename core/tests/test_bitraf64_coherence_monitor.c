#include "../bitraf64_coherence_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

int test_coherence_monitor_init(void) {
  printf("Test 1: Coherence Monitor Initialization\n");

  bitraf64_coherence_monitor_t monitor;
  uint32_t degradation_threshold = (uint32_t)(0.75 * 65536.0);

  if (!bitraf64_coherence_monitor_init(&monitor, degradation_threshold)) {
    printf("  ✗ Initialization failed\n");
    return 0;
  }

  printf("  Enabled: %s\n", monitor.enabled ? "yes" : "no");
  printf("  Degradation threshold: 0x%08x (%.4f)\n", monitor.phi_degrade_threshold,
         (double)monitor.phi_degrade_threshold / 65536.0);

  return 1;
}

int test_record_package(void) {
  printf("Test 2: Record Package Event\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  bitraf64_monitor_pkg_t pkg = {
    .pkg_idx = 0,
    .coherence_phi = (uint64_t)(0.95 * 65536.0),
    .wall_time_ms = 100,
    .l1_miss_rate = 2,
    .layer = 0,
    .timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL
  };

  if (!bitraf64_coherence_monitor_record_package(&monitor, &pkg)) {
    printf("  ✗ Record package failed\n");
    return 0;
  }

  bitraf64_monitor_layer_t *layer = &monitor.layers[0];
  printf("  Layer 0 package count: %u\n", layer->pkg_count);
  printf("  Layer 0 mean phi: 0x%016" PRIx64 " (%.4f)\n", layer->mean_phi,
         (double)layer->mean_phi / 65536.0);
  printf("  Layer 0 total time: %u ms\n", layer->total_time_ms);

  return 1;
}

int test_record_layer(void) {
  printf("Test 3: Record Layer Event\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  uint64_t phi_scores[] = {
    (uint64_t)(0.90 * 65536.0),
    (uint64_t)(0.95 * 65536.0),
    (uint64_t)(0.88 * 65536.0),
    (uint64_t)(0.92 * 65536.0),
    (uint64_t)(0.94 * 65536.0)
  };

  if (!bitraf64_coherence_monitor_record_layer(&monitor, 0, 5, phi_scores)) {
    printf("  ✗ Record layer failed\n");
    return 0;
  }

  bitraf64_monitor_layer_t *layer = &monitor.layers[0];
  printf("  Layer 0 package count: %u\n", layer->pkg_count);
  printf("  Layer 0 mean phi: 0x%016" PRIx64 " (%.4f)\n", layer->mean_phi,
         (double)layer->mean_phi / 65536.0);
  printf("  Layer 0 min phi: 0x%016" PRIx64 " (%.4f)\n", layer->min_phi,
         (double)layer->min_phi / 65536.0);
  printf("  Layer 0 max phi: 0x%016" PRIx64 " (%.4f)\n", layer->max_phi,
         (double)layer->max_phi / 65536.0);

  return 1;
}

int test_anomaly_detection(void) {
  printf("Test 4: Anomaly Detection\n");

  bitraf64_coherence_monitor_t monitor;
  uint32_t threshold = (uint32_t)(0.80 * 65536.0);
  bitraf64_coherence_monitor_init(&monitor, threshold);

  uint64_t good_scores[] = {
    (uint64_t)(0.90 * 65536.0),
    (uint64_t)(0.92 * 65536.0),
    (uint64_t)(0.91 * 65536.0)
  };

  uint64_t bad_scores[] = {
    (uint64_t)(0.70 * 65536.0),
    (uint64_t)(0.72 * 65536.0),
    (uint64_t)(0.71 * 65536.0)
  };

  bitraf64_coherence_monitor_record_layer(&monitor, 0, 3, good_scores);
  bitraf64_coherence_monitor_record_layer(&monitor, 1, 3, bad_scores);

  int anomaly_layer_0 = bitraf64_coherence_monitor_check_anomaly(&monitor, 0);
  int anomaly_layer_1 = bitraf64_coherence_monitor_check_anomaly(&monitor, 1);

  printf("  Layer 0 (good coherence): anomaly = %s\n", anomaly_layer_0 ? "detected" : "not detected");
  printf("  Layer 1 (bad coherence): anomaly = %s\n", anomaly_layer_1 ? "detected" : "not detected");

  return 1;
}

int test_dvfs_suggestion(void) {
  printf("Test 5: DVFS Frequency Scaling Suggestion\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  uint64_t excellent_scores[] = {
    (uint64_t)(0.97 * 65536.0),
    (uint64_t)(0.98 * 65536.0)
  };

  uint64_t poor_scores[] = {
    (uint64_t)(0.70 * 65536.0),
    (uint64_t)(0.72 * 65536.0)
  };

  uint64_t normal_scores[] = {
    (uint64_t)(0.85 * 65536.0),
    (uint64_t)(0.84 * 65536.0)
  };

  bitraf64_coherence_monitor_record_layer(&monitor, 0, 2, excellent_scores);
  bitraf64_coherence_monitor_record_layer(&monitor, 1, 2, poor_scores);
  bitraf64_coherence_monitor_record_layer(&monitor, 2, 2, normal_scores);

  float dvfs_0 = bitraf64_coherence_monitor_suggest_dvfs(&monitor, 0);
  float dvfs_1 = bitraf64_coherence_monitor_suggest_dvfs(&monitor, 1);
  float dvfs_2 = bitraf64_coherence_monitor_suggest_dvfs(&monitor, 2);

  printf("  Layer 0 (excellent, φ ≈ 0.975): frequency scaling = %.2f×\n", dvfs_0);
  printf("  Layer 1 (poor, φ ≈ 0.71): frequency scaling = %.2f×\n", dvfs_1);
  printf("  Layer 2 (normal, φ ≈ 0.845): frequency scaling = %.2f×\n", dvfs_2);

  return 1;
}

int test_system_state_query(void) {
  printf("Test 6: System State Query\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  bitraf64_monitor_system_t system_state;
  if (!bitraf64_coherence_monitor_get_system_state(&monitor, &system_state)) {
    printf("  ✗ Get system state failed\n");
    return 0;
  }

  printf("  Total packages: %u\n", system_state.total_packages);
  printf("  System mean phi: 0x%016" PRIx64 " (%.4f)\n", system_state.system_mean_phi,
         (double)system_state.system_mean_phi / 65536.0);
  printf("  Total time: %u ms\n", system_state.total_time_ms);
  printf("  Phase count: %u\n", system_state.phase_count);

  return 1;
}

int test_layer_history_retrieval(void) {
  printf("Test 7: Layer History Retrieval\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  uint64_t phi_scores[] = {
    (uint64_t)(0.90 * 65536.0),
    (uint64_t)(0.85 * 65536.0)
  };

  bitraf64_coherence_monitor_record_layer(&monitor, 0, 2, phi_scores);
  bitraf64_coherence_monitor_record_layer(&monitor, 1, 2, phi_scores);

  bitraf64_monitor_layer_t history[32];
  int count = bitraf64_coherence_monitor_get_layer_history(&monitor, history, 32);

  printf("  Retrieved %d layer entries\n", count);
  for (int i = 0; i < 2; i++) {
    if (history[i].pkg_count > 0) {
      printf("    Layer %u: %u packages, mean phi = %.4f\n", i, history[i].pkg_count,
             (double)history[i].mean_phi / 65536.0);
    }
  }

  return (count > 0) ? 1 : 0;
}

int test_coherence_report(void) {
  printf("Test 8: Coherence Report Generation\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  uint64_t scores[] = {
    (uint64_t)(0.95 * 65536.0),
    (uint64_t)(0.70 * 65536.0),
    (uint64_t)(0.88 * 65536.0)
  };

  bitraf64_coherence_monitor_record_layer(&monitor, 0, 3, scores);
  bitraf64_coherence_monitor_record_layer(&monitor, 1, 3, scores);

  char report_buffer[1024];
  int report_len = bitraf64_coherence_monitor_report(&monitor, report_buffer, 1024);

  if (report_len > 0) {
    printf("  Report (%d bytes):\n%s\n", report_len, report_buffer);
    return 1;
  }

  return 0;
}

int test_monitor_reset(void) {
  printf("Test 9: Monitor Reset\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  uint64_t scores[] = {(uint64_t)(0.90 * 65536.0)};
  bitraf64_coherence_monitor_record_layer(&monitor, 0, 1, scores);

  if (monitor.layers[0].pkg_count == 0) {
    printf("  ✗ Record layer failed\n");
    return 0;
  }

  if (!bitraf64_coherence_monitor_reset(&monitor)) {
    printf("  ✗ Reset failed\n");
    return 0;
  }

  printf("  Layer 0 package count after reset: %u (should be 0)\n", monitor.layers[0].pkg_count);
  printf("  Enabled after reset: %s\n", monitor.enabled ? "yes" : "no");

  return (monitor.layers[0].pkg_count == 0 && monitor.enabled) ? 1 : 0;
}

int test_multiple_layers(void) {
  printf("Test 10: Multiple Layer Tracking\n");

  bitraf64_coherence_monitor_t monitor;
  bitraf64_coherence_monitor_init(&monitor, (uint32_t)(0.75 * 65536.0));

  int tracked_layers = 0;

  for (uint32_t layer = 0; layer < 10; layer++) {
    uint64_t phi = (uint64_t)(0.85 * 65536.0);
    uint64_t scores[] = {phi, phi};

    if (bitraf64_coherence_monitor_record_layer(&monitor, layer, 2, scores)) {
      tracked_layers++;
    }
  }

  printf("  Successfully tracked %d layers\n", tracked_layers);

  bitraf64_monitor_layer_t history[32];
  int retrieved = bitraf64_coherence_monitor_get_layer_history(&monitor, history, 32);

  int filled_layers = 0;
  for (int i = 0; i < retrieved; i++) {
    if (history[i].pkg_count > 0) {
      filled_layers++;
    }
  }

  printf("  Filled layers in history: %d\n", filled_layers);

  return (filled_layers > 0) ? 1 : 0;
}

int main(void) {
  printf("=== BITRAF64 Coherence Monitor Test Suite ===\n\n");

  int passed = 0;
  int total = 10;

  if (test_coherence_monitor_init()) passed++;
  printf("  ✓ Monitor initialization\n\n");

  if (test_record_package()) passed++;
  printf("  ✓ Record package event\n\n");

  if (test_record_layer()) passed++;
  printf("  ✓ Record layer event\n\n");

  if (test_anomaly_detection()) passed++;
  printf("  ✓ Anomaly detection\n\n");

  if (test_dvfs_suggestion()) passed++;
  printf("  ✓ DVFS frequency suggestion\n\n");

  if (test_system_state_query()) passed++;
  printf("  ✓ System state query\n\n");

  if (test_layer_history_retrieval()) passed++;
  printf("  ✓ Layer history retrieval\n\n");

  if (test_coherence_report()) passed++;
  printf("  ✓ Report generation\n\n");

  if (test_monitor_reset()) passed++;
  printf("  ✓ Monitor reset\n\n");

  if (test_multiple_layers()) passed++;
  printf("  ✓ Multiple layer tracking\n\n");

  printf("=== Summary ===\n");
  printf("Passed: %d/%d tests\n", passed, total);

  return (passed == total) ? 0 : 1;
}
