#include "../hardware_tuning.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static int test_soc_detection(void) {
  printf("\n=== Sprint 5.1: SOC Detection ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  assert(soc >= TERMUX_SOC_UNKNOWN && soc <= TERMUX_SOC_EXYNOS_2200);

  printf("  Detected SOC: %s\n", termux_hardware_soc_name(soc));
  printf("✓ SOC detection PASSED\n");
  return 0;
}

static int test_hardware_config(void) {
  printf("\n=== Sprint 5.2: Hardware Configuration ===\n");

  struct termux_hardware_config config = {};
  int ret = termux_hardware_get_config(&config);
  assert(ret == 0);
  assert(config.total_cores > 0);
  assert(config.total_cores <= 8);
  assert(config.big_cores > 0);
  assert(config.max_freq_mhz > 1000);

  printf("  Total cores: %u\n", config.total_cores);
  printf("  Big cores: %u, Little cores: %u\n", config.big_cores, config.little_cores);
  printf("  Max frequency: %u MHz\n", config.max_freq_mhz);
  printf("  Features: NEON=%u, SVE=%u, AVX512=%u\n",
         config.has_neon, config.has_sve, config.has_avx512);

  printf("✓ Hardware config PASSED\n");
  return 0;
}

static int test_platform_profile_init(void) {
  printf("\n=== Sprint 5.3: Platform Profile Initialization ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  hardware_context_t ctx = {};

  int ret = termux_platform_profile_init(&ctx, soc);
  assert(ret == 0);
  assert(ctx.profile.core_count > 0);
  assert(ctx.profile.max_freq_mhz > 0);

  printf("  Platform: %s\n", termux_hardware_soc_name(ctx.profile.soc_type));
  printf("  Cores: %u\n", ctx.profile.core_count);
  printf("  Max freq: %u MHz\n", ctx.profile.max_freq_mhz);

  printf("✓ Platform profile init PASSED\n");
  return 0;
}

static int test_golden_core_prioritization(void) {
  printf("\n=== Sprint 5.4: Golden Core Prioritization ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  hardware_context_t ctx = {};

  int ret = termux_platform_profile_init(&ctx, soc);
  assert(ret == 0);

  uint32_t baseline_freq = ctx.core_frequencies[ctx.profile.golden_core_id];

  for (uint32_t pkg_idx = 0; pkg_idx < 10; pkg_idx++) {
    ret = termux_golden_core_prioritize(&ctx, pkg_idx);
    assert(ret == 0 || ret == -2);
  }

  uint32_t golden_freq = ctx.core_frequencies[ctx.profile.golden_core_id];
  assert(golden_freq >= baseline_freq);

  printf("  Baseline golden core freq: %u MHz\n", baseline_freq);
  printf("  After prioritization: %u MHz\n", golden_freq);
  printf("  Scheduled packages: %u\n", ctx.scheduled_count);

  printf("✓ Golden core prioritization PASSED\n");
  return 0;
}

static int test_efficiency_core_bypass(void) {
  printf("\n=== Sprint 5.5: Efficiency Core Bypass ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  hardware_context_t ctx = {};

  int ret = termux_platform_profile_init(&ctx, soc);
  if (ret != 0 || ctx.profile.effi_core_mask == 0) {
    printf("  No efficiency cores available, skipping\n");
    return 0;
  }

  for (uint32_t pkg_idx = 0; pkg_idx < 5; pkg_idx++) {
    ret = termux_efficiency_core_bypass(&ctx, pkg_idx);
    assert(ret == 0);
  }

  uint32_t highest_util = 0;
  for (uint32_t i = 0; i < ctx.profile.core_count; i++) {
    if (ctx.core_utilization[i] > highest_util) {
      highest_util = ctx.core_utilization[i];
    }
  }

  printf("  Highest core utilization: %u packages\n", highest_util);
  printf("✓ Efficiency core bypass PASSED\n");
  return 0;
}

static int test_big_little_load_balance(void) {
  printf("\n=== Sprint 5.6: Big.LITTLE Load Balancing ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  hardware_context_t ctx = {};

  int ret = termux_platform_profile_init(&ctx, soc);
  assert(ret == 0);

  for (uint32_t i = 0; i < 8; i++) {
    ctx.core_utilization[i] = (i < 4) ? 10 : 2;
  }

  ret = termux_big_little_load_balance(&ctx, 0);
  assert(ret == 0);

  printf("  Performed load balancing on layer 0\n");
  printf("✓ Big.LITTLE load balance PASSED\n");
  return 0;
}

static int test_neon_mapping(void) {
  printf("\n=== Sprint 5.7: NEON Mapping Optimization ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  hardware_context_t ctx = {};

  int ret = termux_platform_profile_init(&ctx, soc);
  if (ret != 0 || !ctx.profile.has_neon) {
    printf("  NEON not available, skipping\n");
    return 0;
  }

  ret = termux_neon_mapping_optimize(&ctx);
  assert(ret == 0);

  printf("  Optimized NEON mapping for %u cores\n", ctx.profile.core_count);
  printf("✓ NEON mapping optimization PASSED\n");
  return 0;
}

static int test_coherence_score_per_platform(void) {
  printf("\n=== Sprint 5.8: Platform Coherence Scoring ===\n");

  enum termux_soc_type soc = termux_hardware_detect_soc();
  hardware_context_t ctx = {};

  int ret = termux_platform_profile_init(&ctx, soc);
  assert(ret == 0);

  double total_coherence = 0.0;
  for (uint32_t i = 0; i < ctx.profile.core_count; i++) {
    double score = termux_coherence_score_platform(&ctx, i);
    assert(score >= 0.0 && score <= 1.0);
    total_coherence += score;
  }

  double avg_coherence = total_coherence / ctx.profile.core_count;
  printf("  Average coherence score: %.3f\n", avg_coherence);

  printf("✓ Platform coherence scoring PASSED\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("                SPRINT 5: HARDWARE-SPECIFIC TUNING\n");
  printf("================================================================================\n");

  int all_passed = 0;
  all_passed += test_soc_detection();
  all_passed += test_hardware_config();
  all_passed += test_platform_profile_init();
  all_passed += test_golden_core_prioritization();
  all_passed += test_efficiency_core_bypass();
  all_passed += test_big_little_load_balance();
  all_passed += test_neon_mapping();
  all_passed += test_coherence_score_per_platform();

  printf("\n================================================================================\n");
  if (all_passed == 0) {
    printf("✓ ALL SPRINT 5 TESTS PASSED\n");
    printf("  SOC detection: ✓\n");
    printf("  Hardware config: ✓\n");
    printf("  Platform profile: ✓\n");
    printf("  Golden core prioritization: ✓\n");
    printf("  Efficiency core bypass: ✓\n");
    printf("  Big.LITTLE load balance: ✓\n");
    printf("  NEON mapping: ✓\n");
    printf("  Coherence scoring: ✓\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");

  return all_passed == 0 ? 0 : 1;
}
