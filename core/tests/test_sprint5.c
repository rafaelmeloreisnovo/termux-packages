#include "../hardware_tuning.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_soc_detection(void) {
  printf("\n=== Sprint 5.1: SoC Identity Probe ===\n");
  enum termux_soc_type soc = termux_hardware_detect_soc();
  assert(soc >= TERMUX_SOC_UNKNOWN && soc <= TERMUX_SOC_EXYNOS_2200);
  printf("  SoC identity: %s\n", termux_hardware_soc_name(soc));
  if (soc == TERMUX_SOC_UNKNOWN)
    printf("  State: TOKEN_VAZIO_SOC_IDENTITY (accepted; no fallback invented)\n");
  else
    printf("  State: OBSERVED_LIMITED\n");
  printf("✓ SoC identity handling PASSED\n");
  return 0;
}

static int test_hardware_config(void) {
  printf("\n=== Sprint 5.2: Observable Hardware Configuration ===\n");
  struct termux_hardware_config config = {};
  int ret = termux_hardware_get_config(&config);
  assert(ret == 0);
  assert(config.total_cores > 0);
  assert(config.total_cores <= TERMUX_MAX_CORES);

  printf("  Online cores observed: %u\n", config.total_cores);
  printf("  Big cores inferred from cpufreq: %u\n", config.big_cores);
  printf("  Little cores inferred from cpufreq: %u\n", config.little_cores);
  if (config.max_freq_mhz > 0)
    printf("  cpufreq max observed: %u MHz\n", config.max_freq_mhz);
  else
    printf("  cpufreq max: TOKEN_VAZIO\n");
  printf("  Feature tokens: NEON=%u SVE=%u AVX512=%u\n",
         config.has_neon, config.has_sve, config.has_avx512);

  if (config.max_freq_mhz == 0) {
    assert(config.big_cores == 0);
    assert(config.little_cores == 0);
    assert(config.golden.count == 0);
  }
  printf("✓ observable hardware config PASSED\n");
  return 0;
}

static int test_platform_profile_init(void) {
  printf("\n=== Sprint 5.3: Observed-Limited Platform Profile ===\n");
  hardware_context_t ctx = {};
  int ret = termux_platform_profile_init(&ctx, termux_hardware_detect_soc());
  assert(ret == 0);
  assert(ctx.profile.core_count > 0);
  assert(ctx.profile.core_count <= TERMUX_MAX_CORES);
  printf("  Platform identity: %s\n", termux_hardware_soc_name(ctx.profile.soc_type));
  printf("  Cores: %u\n", ctx.profile.core_count);
  printf("  Max freq: %s\n", ctx.profile.max_freq_mhz > 0 ? "OBSERVED" : "TOKEN_VAZIO");
  printf("✓ profile scope PASSED (no device claim)\n");
  return 0;
}

static int test_golden_core_prioritization(void) {
  printf("\n=== Sprint 5.4: Golden-Core Model Gate ===\n");
  hardware_context_t ctx = {};
  int ret = termux_platform_profile_init(&ctx, termux_hardware_detect_soc());
  assert(ret == 0);

  if (ctx.profile.perf_core_mask == 0 || ctx.profile.max_freq_mhz == 0) {
    ret = termux_golden_core_prioritize(&ctx, 0);
    assert(ret == -2);
    printf("  BLOCKED: no observed heterogeneous topology/frequency\n");
    printf("✓ fail-closed golden-core gate PASSED\n");
    return 0;
  }

  ret = termux_golden_core_prioritize(&ctx, 0);
  assert(ret == 0);
  printf("  Scheduler model accepted observed topology\n");
  printf("✓ golden-core model PASSED (not DVFS proof)\n");
  return 0;
}

static int test_efficiency_core_bypass(void) {
  printf("\n=== Sprint 5.5: Efficiency-Core Model Gate ===\n");
  hardware_context_t ctx = {};
  int ret = termux_platform_profile_init(&ctx, termux_hardware_detect_soc());
  assert(ret == 0);
  ret = termux_efficiency_core_bypass(&ctx, 0);
  if (ctx.profile.effi_core_mask == 0) {
    assert(ret == -2);
    printf("  BLOCKED: no observed efficiency-core mask\n");
  } else {
    assert(ret == 0);
    printf("  Model used observed efficiency-core mask\n");
  }
  printf("✓ efficiency-core gate PASSED\n");
  return 0;
}

static int test_big_little_load_balance(void) {
  printf("\n=== Sprint 5.6: Big/LITTLE Model Scope ===\n");
  hardware_context_t ctx = {};
  int ret = termux_platform_profile_init(&ctx, termux_hardware_detect_soc());
  assert(ret == 0);
  ret = termux_big_little_load_balance(&ctx, 0);
  assert(ret == 0);
  if (ctx.profile.perf_core_mask == 0 || ctx.profile.effi_core_mask == 0)
    printf("  No observed heterogeneous split; no balancing claim made\n");
  else
    printf("  Observed masks available; scheduler model exercised\n");
  printf("✓ big/LITTLE scope PASSED\n");
  return 0;
}

static int test_neon_mapping(void) {
  printf("\n=== Sprint 5.7: NEON Eligibility Gate ===\n");
  hardware_context_t ctx = {};
  int ret = termux_platform_profile_init(&ctx, termux_hardware_detect_soc());
  assert(ret == 0);
  ret = termux_neon_mapping_optimize(&ctx);
  if (!ctx.profile.has_neon) {
    assert(ret == -1);
    printf("  NEON token not observed; optimization blocked\n");
  } else {
    assert(ret == 0);
    printf("  NEON/ASIMD token observed; model path eligible\n");
  }
  printf("✓ NEON evidence gate PASSED (instruction execution not claimed)\n");
  return 0;
}

static int test_coherence_score_per_platform(void) {
  printf("\n=== Sprint 5.8: Platform Score Scope ===\n");
  hardware_context_t ctx = {};
  int ret = termux_platform_profile_init(&ctx, termux_hardware_detect_soc());
  assert(ret == 0);
  for (uint32_t i = 0; i < ctx.profile.core_count; i++) {
    double score = termux_coherence_score_platform(&ctx, i);
    assert(score >= 0.0 && score <= 1.0);
  }
  printf("✓ score remains bounded; no performance claim promoted\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("       SPRINT 5: OBSERVED-LIMITED HARDWARE MODEL (NOT DEVICE VALIDATION)\n");
  printf("================================================================================\n");
  printf("claim_allowed=false\n");
  printf("physical_device_verified=false\n");

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
    printf("✓ ALL SPRINT 5 SCOPE/REGRESSION TESTS PASSED\n");
    printf("  PRODUCT_READINESS=NOT_CLAIMED\n");
    printf("  DEVICE_RUNTIME=TOKEN_VAZIO_UNLESS_SEPARATE_RECEIPT\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");
  return all_passed == 0 ? 0 : 1;
}
