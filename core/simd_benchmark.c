#include "build_orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SIMD_BENCHMARK_BATCHES 32
#define SIMD_PACKAGES_PER_BATCH 4

typedef struct {
  uint64_t wall_time_ns;
  uint32_t total_cycles;
  uint64_t total_phi;
  double speedup_factor;
} simd_benchmark_result_t;

static inline uint64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void benchmark_sequential(struct termux_orchestrator *orch,
                                 simd_benchmark_result_t *result) {
  printf("\n=== Sequential Execution (4 packages one-by-one) ===\n");

  uint64_t start_ns = get_time_ns();
  uint64_t total_phi = 0;
  uint32_t total_cycles = 0;

  for (uint32_t batch = 0; batch < SIMD_BENCHMARK_BATCHES; batch++) {
    for (uint32_t pkg = 0; pkg < SIMD_PACKAGES_PER_BATCH; pkg++) {
      char pkg_name[64];
      snprintf(pkg_name, sizeof(pkg_name), "seq-batch%u-pkg%u", batch, pkg);

      int ret = termux_orchestrator_execute(orch, pkg_name,
                                            batch * SIMD_PACKAGES_PER_BATCH + pkg);
      if (ret != 0) {
        printf("  ✗ Package failed: %d\n", ret);
        return;
      }

      total_phi += orch->state.coherence_phi;
      total_cycles += orch->state.cycle_count;
    }

    if ((batch + 1) % 8 == 0) {
      printf("  Progress: %u/%u batches\n", batch + 1, SIMD_BENCHMARK_BATCHES);
    }
  }

  uint64_t end_ns = get_time_ns();
  result->wall_time_ns = end_ns - start_ns;
  result->total_cycles = total_cycles;
  result->total_phi = total_phi;
  result->speedup_factor = 1.0;

  printf("  Total wall time: %.2f ms\n", (double)result->wall_time_ns / 1e6);
  printf("  Total cycles: %u\n", result->total_cycles);
  printf("  Mean φ: %.4f\n", (double)total_phi / (double)(SIMD_BENCHMARK_BATCHES * SIMD_PACKAGES_PER_BATCH) / (double)(1ULL << 16));
}

static void benchmark_simd_4way(struct termux_orchestrator *orch,
                                simd_benchmark_result_t *result) {
  printf("\n=== SIMD 4-Way Vectorization ===\n");

  uint64_t start_ns = get_time_ns();
  uint64_t total_phi = 0;
  uint32_t total_cycles = 0;

  for (uint32_t batch = 0; batch < SIMD_BENCHMARK_BATCHES; batch++) {
    const char *pkg_names[4];
    uint32_t pkg_indices[4];

    for (uint32_t i = 0; i < SIMD_PACKAGES_PER_BATCH; i++) {
      static char buffer[4][64];
      snprintf(buffer[i], sizeof(buffer[i]), "simd-batch%u-pkg%u", batch, i);
      pkg_names[i] = buffer[i];
      pkg_indices[i] = batch * SIMD_PACKAGES_PER_BATCH + i;
    }

    int ret = termux_orchestrator_execute_simd_4way(orch, pkg_names, pkg_indices, 4);
    if (ret != 0) {
      printf("  ✗ SIMD batch failed: %d\n", ret);
      return;
    }

    total_phi += orch->state.coherence_phi;
    total_cycles += orch->state.cycle_count;

    if ((batch + 1) % 8 == 0) {
      printf("  Progress: %u/%u batches\n", batch + 1, SIMD_BENCHMARK_BATCHES);
    }
  }

  uint64_t end_ns = get_time_ns();
  result->wall_time_ns = end_ns - start_ns;
  result->total_cycles = total_cycles;
  result->total_phi = total_phi;

  printf("  Total wall time: %.2f ms\n", (double)result->wall_time_ns / 1e6);
  printf("  Total cycles: %u\n", result->total_cycles);
  printf("  Mean φ: %.4f\n", (double)total_phi / (double)(SIMD_BENCHMARK_BATCHES * SIMD_PACKAGES_PER_BATCH) / (double)(1ULL << 16));
}

static void print_simd_comparison(simd_benchmark_result_t *sequential,
                                  simd_benchmark_result_t *simd) {
  printf("\n");
  printf("================================================================================\n");
  printf("                    SIMD VECTORIZATION COMPARISON ANALYSIS\n");
  printf("================================================================================\n");

  double speedup = (double)sequential->wall_time_ns / (double)simd->wall_time_ns;
  double cycle_reduction = (double)(sequential->total_cycles - simd->total_cycles) /
                          (double)sequential->total_cycles * 100.0;

  printf("\nWall-Clock Performance:\n");
  printf("  Sequential: %.4f ms\n", (double)sequential->wall_time_ns / 1e6);
  printf("  SIMD 4-way: %.4f ms\n", (double)simd->wall_time_ns / 1e6);
  printf("  Speedup: %.2fx\n", speedup);
  printf("  Improvement: %+.1f%%\n", (speedup - 1.0) * 100.0);

  printf("\nCycle Count Comparison:\n");
  printf("  Sequential: %u cycles\n", sequential->total_cycles);
  printf("  SIMD 4-way: %u cycles\n", simd->total_cycles);
  printf("  Reduction: %.1f%%\n", cycle_reduction);

  printf("\nThroughput:\n");
  uint32_t total_packages = SIMD_BENCHMARK_BATCHES * SIMD_PACKAGES_PER_BATCH;
  double seq_throughput = (double)total_packages / ((double)sequential->wall_time_ns / 1e9);
  double simd_throughput = (double)total_packages / ((double)simd->wall_time_ns / 1e9);
  printf("  Sequential: %.0f packages/second\n", seq_throughput);
  printf("  SIMD 4-way: %.0f packages/second\n", simd_throughput);
  printf("  Improvement: %.0f more packages/second\n", simd_throughput - seq_throughput);

  printf("\n================================================================================\n");

  if (speedup > 3.5) {
    printf("✓ EXCELLENT: SIMD 4-way achieves %.2fx speedup (near-optimal 4x)\n", speedup);
  } else if (speedup > 3.0) {
    printf("✓ VERY GOOD: SIMD 4-way achieves %.2fx speedup\n", speedup);
  } else if (speedup > 2.0) {
    printf("✓ GOOD: SIMD 4-way achieves %.2fx speedup\n", speedup);
  } else if (speedup > 1.5) {
    printf("⚠ MODERATE: SIMD 4-way achieves %.2fx speedup (improve vectorization)\n", speedup);
  } else {
    printf("✗ LOW: SIMD 4-way achieves %.2fx speedup (optimize SIMD code)\n", speedup);
  }

  double avg_seq_phi = (double)sequential->total_phi / (double)(SIMD_BENCHMARK_BATCHES * SIMD_PACKAGES_PER_BATCH) / (double)(1ULL << 16);
  double avg_simd_phi = (double)simd->total_phi / (double)(SIMD_BENCHMARK_BATCHES * SIMD_PACKAGES_PER_BATCH) / (double)(1ULL << 16);

  printf("\nCoherence Metric Φ:\n");
  printf("  Sequential: %.4f\n", avg_seq_phi);
  printf("  SIMD 4-way: %.4f\n", avg_simd_phi);

  if (avg_simd_phi > 0.95) {
    printf("  ✓ TARGET ACHIEVED: Φ > 0.95\n");
  } else if (avg_simd_phi > 0.90) {
    printf("  ✓ CLOSE TO TARGET: Φ > 0.90 (Phase 9.5 achieved)\n");
  } else {
    printf("  ⚠ APPROACHING TARGET: Φ = %.4f\n", avg_simd_phi);
  }

  printf("================================================================================\n\n");
}

static void print_simd_features(void) {
  printf("\nSIMD Backend Support:\n");

  if (termux_orchestrator_has_simd_support()) {
    printf("  ✓ SIMD Support: Enabled\n");
    printf("  Backend: %s\n", termux_orchestrator_simd_backend());
  } else {
    printf("  ✗ SIMD Support: Disabled (Generic fallback)\n");
  }

  printf("\nExpected 4-Way Vectorization Gains:\n");
  printf("  Throughput: 4x improvement (ideal)\n");
  printf("  Latency per package: ~4x reduction\n");
  printf("  L1 cache utilization: 4× more compact\n");
  printf("  Memory bandwidth: 4x more efficient\n");
}

int main(void) {
  printf("\n");
  printf("================================================================================\n");
  printf("           TERMUX-PACKAGES SIMD VECTORIZATION BENCHMARK (Phase 9.6)\n");
  printf("================================================================================\n");

  struct termux_orchestrator orch_seq = {};
  struct termux_orchestrator orch_simd = {};

  termux_orchestrator_init(&orch_seq);
  termux_orchestrator_init(&orch_simd);

  simd_benchmark_result_t sequential = {};
  simd_benchmark_result_t simd_4way = {};

  benchmark_sequential(&orch_seq, &sequential);
  benchmark_simd_4way(&orch_simd, &simd_4way);

  print_simd_comparison(&sequential, &simd_4way);
  print_simd_features();

  double final_speedup = (double)sequential.wall_time_ns / (double)simd_4way.wall_time_ns;
  printf("Final Verdict: %.2fx total speedup achieved\n\n", final_speedup);

  return 0;
}
