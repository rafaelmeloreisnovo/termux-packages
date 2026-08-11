#include "../adaptive_dvfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

static int test_dvfs_init(void) {
  printf("\n=== Sprint 11.1: DVFS Initialization ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 4);
  assert(ret == 0);
  assert(dvfs.domain_count == 4);

  for (uint32_t i = 0; i < 4; i++) {
    assert(dvfs.domains[i].domain_id == i);
    assert(dvfs.domains[i].current_level == DVFS_LEVEL_MEDIUM);
    assert(dvfs.domains[i].current_freq_mhz == 1800);
  }

  printf("✓ DVFS initialized with %u frequency domains\n", dvfs.domain_count);
  return 0;
}

static int test_frequency_scaling(void) {
  printf("\n=== Sprint 11.2: Frequency Scaling ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 2);
  assert(ret == 0);

  ret = termux_dvfs_set_frequency(&dvfs, 0, DVFS_LEVEL_MAXIMUM);
  assert(ret == 0);
  assert(termux_dvfs_get_frequency(&dvfs, 0) == DVFS_LEVEL_MAXIMUM);
  assert(dvfs.domains[0].current_freq_mhz == 3200);

  ret = termux_dvfs_set_frequency(&dvfs, 0, DVFS_LEVEL_MINIMAL);
  assert(ret == 0);
  assert(termux_dvfs_get_frequency(&dvfs, 0) == DVFS_LEVEL_MINIMAL);
  assert(dvfs.domains[0].current_freq_mhz == 800);

  printf("✓ Frequency scaling working (3200 MHz → 800 MHz)\n");
  return 0;
}

static int test_coherence_adjustment(void) {
  printf("\n=== Sprint 11.3: Coherence-Based Adjustment ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 1);
  assert(ret == 0);

  dvfs.phi_high_threshold = 0.90;
  dvfs.phi_low_threshold = 0.70;

  ret = termux_dvfs_adjust_for_coherence(&dvfs, 0, 0.95);
  assert(ret == 0);
  assert(termux_dvfs_get_frequency(&dvfs, 0) == DVFS_LEVEL_LOW);

  ret = termux_dvfs_adjust_for_coherence(&dvfs, 0, 0.60);
  assert(ret == 0);
  assert(termux_dvfs_get_frequency(&dvfs, 0) == DVFS_LEVEL_MEDIUM);

  printf("✓ Coherence adjustment working (high φ → scale down, low φ → scale up)\n");
  return 0;
}

static int test_threshold_management(void) {
  printf("\n=== Sprint 11.4: Threshold Management ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 1);
  assert(ret == 0);

  ret = termux_dvfs_set_thresholds(&dvfs, 0.92, 0.72);
  assert(ret == 0);
  assert(dvfs.phi_high_threshold == 0.92);
  assert(dvfs.phi_low_threshold == 0.72);

  ret = termux_dvfs_set_thresholds(&dvfs, 0.5, 0.8);
  assert(ret == -1);

  printf("✓ Threshold management working\n");
  return 0;
}

static int test_energy_estimation(void) {
  printf("\n=== Sprint 11.5: Energy Estimation ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 2);
  assert(ret == 0);

  ret = termux_dvfs_set_frequency(&dvfs, 0, DVFS_LEVEL_MAXIMUM);
  assert(ret == 0);

  uint64_t energy_max = termux_dvfs_estimate_energy(&dvfs, 0, 1000000);
  printf("  Energy at 3200 MHz for 1M cycles: %" PRIu64 " µJ\n", energy_max);

  ret = termux_dvfs_set_frequency(&dvfs, 0, DVFS_LEVEL_MINIMAL);
  assert(ret == 0);

  uint64_t energy_min = termux_dvfs_estimate_energy(&dvfs, 0, 1000000);
  printf("  Energy at 800 MHz for 1M cycles: %" PRIu64 " µJ\n", energy_min);

  assert(energy_max > energy_min);
  printf("✓ Energy estimation working (lower frequency = lower energy)\n");
  return 0;
}

static int test_multi_domain_scaling(void) {
  printf("\n=== Sprint 11.6: Multi-Domain Scaling ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 4);
  assert(ret == 0);

  for (uint32_t i = 0; i < 4; i++) {
    dvfs_level_t level = (dvfs_level_t)(i % TERMUX_MAX_FREQUENCY_LEVELS);
    ret = termux_dvfs_set_frequency(&dvfs, i, level);
    assert(ret == 0);
  }

  printf("  Domain frequencies:\n");
  for (uint32_t i = 0; i < 4; i++) {
    dvfs_level_t level = termux_dvfs_get_frequency(&dvfs, i);
    printf("    Domain %u: Level %u (%u MHz)\n", i, level,
           dvfs.domains[i].current_freq_mhz);
  }

  printf("✓ Multi-domain scaling working\n");
  return 0;
}

static int test_scaling_statistics(void) {
  printf("\n=== Sprint 11.7: Scaling Statistics ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 2);
  assert(ret == 0);

  for (int i = 0; i < 5; i++) {
    dvfs_level_t level = (dvfs_level_t)((i % TERMUX_MAX_FREQUENCY_LEVELS));
    termux_dvfs_set_frequency(&dvfs, 0, level);
  }

  uint64_t scaling_events = dvfs.scaling_events;
  printf("  Scaling events: %" PRIu64 "\n", scaling_events);
  assert(scaling_events > 0);

  printf("✓ Scaling statistics tracking working\n");
  return 0;
}

static int test_efficiency_metric(void) {
  printf("\n=== Sprint 11.8: Efficiency Metric ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 1);
  assert(ret == 0);

  dvfs.domains[0].coherence_phi = 0.90;
  termux_dvfs_estimate_energy(&dvfs, 0, 1000000);

  double efficiency = termux_dvfs_efficiency_metric(&dvfs);
  printf("  Efficiency metric: %.6f\n", efficiency);

  assert(efficiency >= 0.0);
  printf("✓ Efficiency metric calculation working\n");
  return 0;
}

static int test_adaptive_scaling_loop(void) {
  printf("\n=== Sprint 11.9: Adaptive Scaling Loop ===\n");

  adaptive_dvfs_t dvfs;
  int ret = termux_adaptive_dvfs_init(&dvfs, 1);
  assert(ret == 0);

  dvfs.phi_high_threshold = 0.85;
  dvfs.phi_low_threshold = 0.75;

  double coherence_values[] = {0.95, 0.88, 0.72, 0.68, 0.78, 0.92, 0.80};

  for (size_t i = 0; i < sizeof(coherence_values) / sizeof(coherence_values[0]); i++) {
    termux_dvfs_adjust_for_coherence(&dvfs, 0, coherence_values[i]);
    dvfs_level_t curr_level = termux_dvfs_get_frequency(&dvfs, 0);

    printf("  φ=%.2f → Level %u (%u MHz)\n", coherence_values[i], curr_level,
           dvfs.domains[0].current_freq_mhz);
  }

  printf("✓ Adaptive scaling loop working\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("                SPRINT 11: ADAPTIVE FREQUENCY SCALING (DVFS)\n");
  printf("================================================================================\n");

  int all_passed = 0;
  all_passed += test_dvfs_init();
  all_passed += test_frequency_scaling();
  all_passed += test_coherence_adjustment();
  all_passed += test_threshold_management();
  all_passed += test_energy_estimation();
  all_passed += test_multi_domain_scaling();
  all_passed += test_scaling_statistics();
  all_passed += test_efficiency_metric();
  all_passed += test_adaptive_scaling_loop();

  printf("\n================================================================================\n");
  if (all_passed == 0) {
    printf("✓ ALL SPRINT 11 TESTS PASSED\n");
    printf("  DVFS initialization: ✓\n");
    printf("  Frequency scaling: ✓\n");
    printf("  Coherence-based adjustment: ✓\n");
    printf("  Threshold management: ✓\n");
    printf("  Energy estimation: ✓\n");
    printf("  Multi-domain scaling: ✓\n");
    printf("  Scaling statistics: ✓\n");
    printf("  Efficiency metric: ✓\n");
    printf("  Adaptive scaling loop: ✓\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");

  return all_passed == 0 ? 0 : 1;
}
