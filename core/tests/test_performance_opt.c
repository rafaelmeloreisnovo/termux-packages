#include "../manifest_simd.h"
#include "../scheduler_adaptive.h"
#include "../alloc_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/* ============================================================================
 * Test Suite: Phase 9.19 Performance Optimization
 * ============================================================================ */

#define TEST_PASS(name) printf("✓ %s\n", name)
#define TEST_FAIL(name, msg) printf("✗ %s: %s\n", name, msg)

static int tests_passed = 0;
static int tests_failed = 0;

/* ============================================================================
 * Test 1: CRC32c Scalar Implementation
 * ============================================================================ */

void test_crc32c_scalar(void) {
  const char *test_data = "Hello, World!";
  uint32_t crc = crc32c_scalar((const uint8_t *)test_data, strlen(test_data));

  if (crc != 0) {
    TEST_PASS("CRC32c Scalar");
    tests_passed++;
  } else {
    TEST_FAIL("CRC32c Scalar", "crc computed as 0");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 2: CRC32c SIMD Implementation
 * ============================================================================ */

void test_crc32c_simd(void) {
  const char *test_data = "Performance Optimization Test";
  uint32_t crc_simd = crc32c_simd((const uint8_t *)test_data, strlen(test_data));

  if (crc_simd != 0) {
    TEST_PASS("CRC32c SIMD");
    tests_passed++;
  } else {
    TEST_FAIL("CRC32c SIMD", "crc computed as 0");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 3: CRC32c Consistency (Scalar vs SIMD)
 * ============================================================================ */

void test_crc32c_consistency(void) {
  uint8_t data[1024];
  for (int i = 0; i < 1024; i++) {
    data[i] = (uint8_t)((i * 17 + 42) & 0xff);
  }

  uint32_t crc_scalar = crc32c_scalar(data, 1024);
  uint32_t crc_simd = crc32c_simd(data, 1024);

  if (crc_scalar == crc_simd) {
    TEST_PASS("CRC32c Consistency");
    tests_passed++;
  } else {
    printf("  Scalar: %08x, SIMD: %08x\n", crc_scalar, crc_simd);
    TEST_FAIL("CRC32c Consistency", "results differ");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 4: Manifest Validation with CRC32c
 * ============================================================================ */

void test_manifest_validate_simd(void) {
  manifest_entry_simd_t entry;
  memset(&entry, 0, sizeof(entry));

  strcpy((char *)entry.name, "test-package");
  strcpy((char *)entry.version, "1.0.0");
  entry.arch_flags = 0x0F;  /* All architectures */
  entry.api_level = 21;
  entry.coherence_phi = (uint64_t)(0.87 * (1ULL << 16));
  entry.toroidal_depth = 5;
  entry.dep_count = 2;
  entry.deps[0] = 10;
  entry.deps[1] = 20;

  /* Compute valid CRC32c */
  entry.crc32c = crc32c_simd((const uint8_t *)entry.deps,
                             entry.dep_count * sizeof(uint16_t));

  int result = manifest_validate_simd(&entry);
  if (result == 0) {
    TEST_PASS("Manifest Validation");
    tests_passed++;
  } else {
    printf("  Error code: %d\n", result);
    TEST_FAIL("Manifest Validation", "validation failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 5: Adaptive Scheduler Priority Queue
 * ============================================================================ */

void test_scheduler_init(void) {
  priority_queue_t sched;
  int result = scheduler_init(&sched, 100);

  if (result == 0 && sched.capacity == 100 && sched.count == 0) {
    TEST_PASS("Scheduler Initialization");
    tests_passed++;
    scheduler_free(&sched);
  } else {
    TEST_FAIL("Scheduler Initialization", "init failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 6: Scheduler Enqueue/Dequeue
 * ============================================================================ */

void test_scheduler_queue_ops(void) {
  priority_queue_t sched;
  scheduler_init(&sched, 10);

  scheduled_task_t task1 = {.pkg_idx = 1, .coherence_phi = (uint64_t)(0.90 * (1ULL << 16)), .layer = 0};
  scheduled_task_t task2 = {.pkg_idx = 2, .coherence_phi = (uint64_t)(0.85 * (1ULL << 16)), .layer = 1};

  if (scheduler_enqueue(&sched, &task1) == 0 && scheduler_enqueue(&sched, &task2) == 0) {
    scheduled_task_t dequeued;
    if (scheduler_dequeue(&sched, &dequeued) == 0) {
      if (dequeued.pkg_idx == 1) {  /* Higher φ should dequeue first */
        TEST_PASS("Scheduler Queue Operations");
        tests_passed++;
      } else {
        printf("  Expected pkg_idx=1, got %u\n", dequeued.pkg_idx);
        TEST_FAIL("Scheduler Queue Operations", "wrong priority order");
        tests_failed++;
      }
    } else {
      TEST_FAIL("Scheduler Queue Operations", "dequeue failed");
      tests_failed++;
    }
  } else {
    TEST_FAIL("Scheduler Queue Operations", "enqueue failed");
    tests_failed++;
  }

  scheduler_free(&sched);
}

/* ============================================================================
 * Test 7: Adaptive Scheduler Priority Adjustment
 * ============================================================================ */

void test_scheduler_dynamic_adjustment(void) {
  priority_queue_t sched;
  scheduler_init(&sched, 10);

  scheduled_task_t task = {.pkg_idx = 99, .coherence_phi = (uint64_t)(0.75 * (1ULL << 16)), .layer = 5};

  (void)scheduler_compute_priority(&task, sched.current_phi);  /* Get initial priority */

  /* Simulate coherence drop below threshold */
  scheduler_update_coherence(&sched, (uint64_t)(0.75 * (1ULL << 16)));

  (void)scheduler_compute_priority(&task, sched.current_phi);  /* Get adjusted priority */

  if (sched.adjustments_made > 0) {
    TEST_PASS("Scheduler Dynamic Adjustment");
    tests_passed++;
  } else {
    printf("  Adjustments made: %u (expected > 0)\n", sched.adjustments_made);
    TEST_FAIL("Scheduler Dynamic Adjustment", "no adjustments triggered");
    tests_failed++;
  }

  scheduler_free(&sched);
}

/* ============================================================================
 * Test 8: Memory Pool System
 * ============================================================================ */

void test_pool_system(void) {
  int result = pool_system_init();

  if (result == 0) {
    TEST_PASS("Memory Pool Initialization");
    tests_passed++;
  } else {
    TEST_FAIL("Memory Pool Initialization", "init failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 9: Pool Allocation and Freeing
 * ============================================================================ */

void test_pool_alloc_free(void) {
  pool_system_init();

  void *ptr256 = pool_alloc(256);
  void *ptr1k = pool_alloc(1024);
  void *ptr4k = pool_alloc(4096);

  if (ptr256 && ptr1k && ptr4k) {
    int r1 = pool_free(ptr256, 256);
    int r2 = pool_free(ptr1k, 1024);
    int r3 = pool_free(ptr4k, 4096);

    if (r1 == 0 && r2 == 0 && r3 == 0) {
      TEST_PASS("Pool Allocation/Freeing");
      tests_passed++;
    } else {
      TEST_FAIL("Pool Allocation/Freeing", "free failed");
      tests_failed++;
    }
  } else {
    TEST_FAIL("Pool Allocation/Freeing", "alloc failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 10: Pool Hit Rate
 * ============================================================================ */

void test_pool_hit_rate(void) {
  pool_system_init();

  /* Allocate multiple small buffers */
  for (int i = 0; i < 32; i++) {
    void *ptr = pool_alloc(256);
    if (ptr) pool_free(ptr, 256);
  }

  pool_stats_t stats;
  if (pool_get_stats(&stats) == 0 && stats.overall_hit_rate > 0.95) {
    TEST_PASS("Pool Hit Rate");
    tests_passed++;
  } else {
    printf("  Hit rate: %.1f%% (expected > 95%%)\n", stats.overall_hit_rate * 100);
    TEST_FAIL("Pool Hit Rate", "hit rate too low");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 11: SIMD Benchmark
 * ============================================================================ */

void test_simd_benchmark(void) {
  simd_benchmark_t bench;
  int result = simd_benchmark_crc32c(1024, 100, &bench);

  if (result == 0 && bench.speedup > 0) {
    TEST_PASS("SIMD Benchmark");
    printf("  Speedup: %.2f×\n", bench.speedup);
    tests_passed++;
  } else {
    TEST_FAIL("SIMD Benchmark", "benchmark failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Test 12: Layer Scheduling
 * ============================================================================ */

void test_layer_scheduling(void) {
  layer_schedule_t layer;
  int result = scheduler_layer_create(&layer, 0, 50);

  if (result == 0) {
    /* Add some packages */
    for (uint32_t i = 0; i < 10 && i < 50; i++) {
      layer.pkg_indices[layer.pkg_count] = i;
      layer.phi_scores[layer.pkg_count] = (uint64_t)((0.70 + i * 0.01) * (1ULL << 16));
      layer.pkg_count++;
    }

    /* Sort by φ */
    int sort_result = scheduler_layer_sort_by_phi(&layer);

    if (sort_result == 0 && layer.pkg_count == 10) {
      TEST_PASS("Layer Scheduling");
      tests_passed++;
    } else {
      TEST_FAIL("Layer Scheduling", "sort failed");
      tests_failed++;
    }

    scheduler_layer_free(&layer);
  } else {
    TEST_FAIL("Layer Scheduling", "layer create failed");
    tests_failed++;
  }
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
  printf("\n=== Phase 9.19: Performance Optimization Test Suite ===\n\n");

  test_crc32c_scalar();
  test_crc32c_simd();
  test_crc32c_consistency();
  test_manifest_validate_simd();
  test_scheduler_init();
  test_scheduler_queue_ops();
  test_scheduler_dynamic_adjustment();
  test_pool_system();
  test_pool_alloc_free();
  test_pool_hit_rate();
  test_simd_benchmark();
  test_layer_scheduling();

  printf("\n=== Summary ===\n");
  printf("Passed: %d/12\n", tests_passed);
  printf("Failed: %d/12\n", tests_failed);

  if (tests_failed == 0) {
    printf("\n✓✓✓ All tests PASSED\n");
    return 0;
  } else {
    printf("\n✗✗✗ Some tests FAILED\n");
    return 1;
  }
}
