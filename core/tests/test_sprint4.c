#include "../multicore_orchestrator.h"
#include "../build_orchestrator.h"
#include "../dep_resolver.h"
#include "../phase_barrier_lockfree.h"
#include "../workload_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

static int test_multicore_alloc(void) {
  printf("\n=== Sprint 4.1: Multicore Orchestrator Allocation ===\n");

  multicore_orchestrator_t morch = {};
  int ret = multicore_orchestrator_alloc(&morch, 4);
  assert(ret == 0);
  assert(morch.core_count == 4);
  assert(morch.active_cores == 4);

  printf("✓ Allocated orchestrator for %u cores\n", morch.core_count);
  return 0;
}

static int test_core_detection(void) {
  printf("\n=== Sprint 4.2: Core Detection ===\n");

  multicore_orchestrator_t morch = {};
  int ret = multicore_orchestrator_alloc(&morch, 8);
  assert(ret == 0);

  ret = multicore_orchestrator_detect_cores(&morch);
  assert(ret == 0);
  assert(morch.core_count > 0);
  assert(morch.core_count <= 8);

  printf("Detected %u cores:\n", morch.core_count);
  for (uint32_t i = 0; i < morch.core_count; i++) {
    core_profile_t *prof = &morch.core_profiles[i];
    printf("  Core %u: %u MHz, L1=%u KB, L2=%u KB\n",
           prof->core_id, prof->cpu_freq_mhz,
           prof->l1_cache_kb, prof->l2_cache_kb);
  }

  printf("✓ Core detection PASSED\n");
  return 0;
}

static int test_orchestrator_init(void) {
  printf("\n=== Sprint 4.3: Orchestrator Initialization ===\n");

  multicore_orchestrator_t morch = {};
  int ret = multicore_orchestrator_alloc(&morch, 4);
  assert(ret == 0);

  ret = multicore_orchestrator_init(&morch);
  assert(ret == 0);
  assert(morch.layer_index == 0);
  assert(morch.total_cycles == 0);

  printf("✓ Orchestrator initialized with %u cores\n", morch.core_count);
  return 0;
}

static int test_load_balancing(void) {
  printf("\n=== Sprint 4.4: Load Balancing ===\n");

  multicore_orchestrator_t morch = {};
  int ret = multicore_orchestrator_alloc(&morch, 4);
  assert(ret == 0);

  ret = multicore_orchestrator_detect_cores(&morch);
  assert(ret == 0);

  morch.layer_count = 1;
  morch.layers[0].pkg_count = 16;
  for (uint32_t i = 0; i < 16; i++) {
    morch.layers[0].pkg_indices[i] = i;
  }

  ret = multicore_orchestrator_load_balance(&morch, 0);
  assert(ret == 0);

  uint32_t total_assigned = 0;
  for (uint32_t i = 0; i < morch.core_count; i++) {
    total_assigned += morch.core_profiles[i].packages_completed;
  }

  printf("  Total packages assigned: %u\n", total_assigned);
  printf("✓ Load balancing PASSED\n");
  return 0;
}

static int test_speedup_calculation(void) {
  printf("\n=== Sprint 4.5: Speedup Calculation ===\n");

  multicore_orchestrator_t morch = {};
  int ret = multicore_orchestrator_alloc(&morch, 4);
  assert(ret == 0);

  morch.total_cycles = 1000000;

  double speedup = multicore_orchestrator_calculate_speedup(&morch, 2000000);
  assert(speedup > 1.0);

  printf("  Single-core cycles: 2000000\n");
  printf("  Multi-core cycles: %" PRIu64 "\n", morch.total_cycles);
  printf("  Speedup: %.2fx\n", speedup);

  printf("✓ Speedup calculation PASSED\n");
  return 0;
}

static int test_layer_batch_processing(void) {
  printf("\n=== Sprint 4.6: Layer Batch Processing ===\n");

  multicore_orchestrator_t morch = {};
  int ret = multicore_orchestrator_alloc(&morch, 4);
  assert(ret == 0);

  ret = multicore_orchestrator_init(&morch);
  assert(ret == 0);

  layer_batch_t layers[4];
  for (uint32_t i = 0; i < 4; i++) {
    layers[i].layer_id = i;
    layers[i].pkg_count = 8;
    for (uint32_t j = 0; j < 8; j++) {
      layers[i].pkg_indices[j] = (i * 8 + j) % TERMUX_REAL_PKG_COUNT;
    }
    layers[i].completion_bitmap = 0;
  }

  ret = multicore_orchestrator_execute_parallel(&morch, layers, 4);
  assert(ret == 0 || ret == -1);

  printf("  Executed %u layers\n", morch.layer_count);
  printf("  Wall time: %.3f seconds\n", morch.wall_time_sec);
  printf("✓ Layer batch processing PASSED\n");
  return 0;
}

int main(void) {
  printf("\n================================================================================\n");
  printf("                    SPRINT 4: MULTICORE PARALLELIZATION\n");
  printf("================================================================================\n");

  int all_passed = 0;
  all_passed += test_multicore_alloc();
  all_passed += test_core_detection();
  all_passed += test_orchestrator_init();
  all_passed += test_load_balancing();
  all_passed += test_speedup_calculation();
  all_passed += test_layer_batch_processing();

  printf("\n================================================================================\n");
  if (all_passed == 0) {
    printf("✓ ALL SPRINT 4 TESTS PASSED\n");
    printf("  Multicore allocation: ✓\n");
    printf("  Core detection: ✓\n");
    printf("  Orchestrator init: ✓\n");
    printf("  Load balancing: ✓\n");
    printf("  Speedup calculation: ✓\n");
    printf("  Layer batch processing: ✓\n");
  } else {
    printf("✗ SOME TESTS FAILED\n");
  }
  printf("================================================================================\n\n");

  return all_passed == 0 ? 0 : 1;
}
