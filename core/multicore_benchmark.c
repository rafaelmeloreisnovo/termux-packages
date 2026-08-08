#include "build_orchestrator.h"
#include "job_scheduler_parallel.h"
#include "cpu_affinity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MULTICORE_BENCHMARK_LAYERS 42
#define MULTICORE_BENCHMARK_PACKAGES_PER_LAYER 49

typedef struct {
  uint64_t wall_time_ns;
  uint32_t total_cycles;
  uint64_t total_phi;
  double mean_phi;
  double speedup_factor;
  uint32_t thread_count;
} multicore_benchmark_result_t;

static inline uint64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int mock_execute_package(uint32_t pkg_idx, struct termux_build_state *state) {
  if (!state) {
    return -1;
  }

  state->pkg_idx = pkg_idx;
  state->phase = 6;
  state->arch_state = 4;

  uint32_t depth = pkg_idx % 42;
  uint32_t depth_score = 42 - (depth * 6 + 4);
  uint64_t coherence_base = (depth_score * 65536ULL) / 42;

  uint32_t gcd_val = 7;
  uint64_t gcd_factor = (((uint64_t)gcd_val) * 65536ULL) / 7;
  uint64_t phi = (coherence_base * gcd_factor / 65536ULL);

  state->coherence_phi = (phi > 1000ULL) ? (phi - 1000ULL) : 0;
  state->cycle_count = 35 + (pkg_idx % 8);

  return 0;
}

static void create_benchmark_layers(struct termux_layer_info *layers,
                                    uint32_t layer_count) {
  for (uint32_t i = 0; i < layer_count; i++) {
    layers[i].layer_id = i;
    layers[i].toroidal_depth = i;
    layers[i].package_count = MULTICORE_BENCHMARK_PACKAGES_PER_LAYER;

    for (uint32_t j = 0; j < layers[i].package_count; j++) {
      layers[i].package_indices[j] = i * MULTICORE_BENCHMARK_PACKAGES_PER_LAYER + j;
    }
  }
}

static void benchmark_sequential(multicore_benchmark_result_t *result) {
  printf("\n=== Sequential Execution (Single-Threaded) ===\n");

  struct termux_layer_info layers[MULTICORE_BENCHMARK_LAYERS];
  create_benchmark_layers(layers, MULTICORE_BENCHMARK_LAYERS);

  uint64_t start_ns = get_time_ns();
  uint64_t total_phi = 0;
  uint32_t total_cycles = 0;

  for (uint32_t layer = 0; layer < MULTICORE_BENCHMARK_LAYERS; layer++) {
    for (uint32_t pkg = 0; pkg < layers[layer].package_count; pkg++) {
      struct termux_build_state state = {};
      mock_execute_package(layers[layer].package_indices[pkg], &state);

      total_phi += state.coherence_phi;
      total_cycles += state.cycle_count;
    }

    if ((layer + 1) % 7 == 0) {
      printf("  Progress: %u/%u layers\n", layer + 1, MULTICORE_BENCHMARK_LAYERS);
    }
  }

  uint64_t end_ns = get_time_ns();
  result->wall_time_ns = end_ns - start_ns;
  result->total_cycles = total_cycles;
  result->total_phi = total_phi;
  result->mean_phi = (double)total_phi / (double)(MULTICORE_BENCHMARK_LAYERS * MULTICORE_BENCHMARK_PACKAGES_PER_LAYER) / (double)(1ULL << 16);
  result->speedup_factor = 1.0;
  result->thread_count = 1;

  printf("  Total wall time: %.2f ms\n", (double)result->wall_time_ns / 1e6);
  printf("  Total cycles: %u\n", result->total_cycles);
  printf("  Mean φ: %.4f\n", result->mean_phi);
}

static void benchmark_multicore(uint32_t thread_count,
                                multicore_benchmark_result_t *result) {
  printf("\n=== Multi-Core Execution (%u Threads) ===\n", thread_count);

  struct termux_job_scheduler scheduler;
  struct termux_layer_info layers[MULTICORE_BENCHMARK_LAYERS];

  create_benchmark_layers(layers, MULTICORE_BENCHMARK_LAYERS);

  if (termux_job_scheduler_init(&scheduler, thread_count, layers, MULTICORE_BENCHMARK_LAYERS) != 0) {
    printf("  ✗ Scheduler initialization failed\n");
    return;
  }

  uint64_t start_ns = get_time_ns();

  if (termux_job_scheduler_run(&scheduler, mock_execute_package) != 0) {
    printf("  ✗ Job scheduler failed\n");
    return;
  }

  uint64_t end_ns = get_time_ns();

  result->wall_time_ns = end_ns - start_ns;
  result->total_cycles = termux_job_scheduler_total_cycles(&scheduler);
  result->total_phi = termux_job_scheduler_total_phi(&scheduler);
  result->mean_phi = termux_job_scheduler_mean_phi(&scheduler);
  result->thread_count = thread_count;

  printf("  Total wall time: %.2f ms\n", (double)result->wall_time_ns / 1e6);
  printf("  Total cycles: %u\n", result->total_cycles);
  printf("  Mean φ: %.4f\n", result->mean_phi);
}

static void print_multicore_comparison(multicore_benchmark_result_t *sequential,
                                       multicore_benchmark_result_t *multicore) {
  printf("\n");
  printf("================================================================================\n");
  printf("                    MULTI-CORE PARALLELIZATION ANALYSIS\n");
  printf("================================================================================\n");

  double speedup = (double)sequential->wall_time_ns / (double)multicore->wall_time_ns;
  double efficiency = speedup / (double)multicore->thread_count * 100.0;

  printf("\nWall-Clock Performance:\n");
  printf("  Sequential:      %.4f ms\n", (double)sequential->wall_time_ns / 1e6);
  printf("  Multi-core (%u):  %.4f ms\n", multicore->thread_count, (double)multicore->wall_time_ns / 1e6);
  printf("  Speedup: %.2fx\n", speedup);
  printf("  Efficiency: %.1f%% (vs ideal %d-way)\n", efficiency, multicore->thread_count);

  printf("\nCycle Count Analysis:\n");
  printf("  Sequential: %u cycles\n", sequential->total_cycles);
  printf("  Multi-core: %u cycles\n", multicore->total_cycles);

  printf("\nCoherence Metric Φ:\n");
  printf("  Sequential: %.4f\n", sequential->mean_phi);
  printf("  Multi-core: %.4f\n", multicore->mean_phi);

  printf("\nThroughput:\n");
  uint32_t total_packages = MULTICORE_BENCHMARK_LAYERS * MULTICORE_BENCHMARK_PACKAGES_PER_LAYER;
  double seq_throughput = (double)total_packages / ((double)sequential->wall_time_ns / 1e9);
  double mc_throughput = (double)total_packages / ((double)multicore->wall_time_ns / 1e9);
  printf("  Sequential: %.0f packages/second\n", seq_throughput);
  printf("  Multi-core: %.0f packages/second\n", mc_throughput);
  printf("  Improvement: %.0f more packages/second\n", mc_throughput - seq_throughput);

  printf("\n================================================================================\n");

  if (speedup > (double)multicore->thread_count * 0.85) {
    printf("✓ EXCELLENT: %u-way parallelization achieves %.2fx speedup (%.1f%% efficiency)\n",
           multicore->thread_count, speedup, efficiency);
  } else if (speedup > (double)multicore->thread_count * 0.70) {
    printf("✓ GOOD: %u-way parallelization achieves %.2fx speedup (%.1f%% efficiency)\n",
           multicore->thread_count, speedup, efficiency);
  } else if (speedup > 1.0) {
    printf("⚠ MODERATE: %u-way parallelization achieves %.2fx speedup (%.1f%% efficiency, consider tuning)\n",
           multicore->thread_count, speedup, efficiency);
  } else {
    printf("✗ LOW: Serialization overhead dominates (speedup %.2fx)\n", speedup);
  }

  printf("================================================================================\n\n");
}

static void print_system_info(void) {
  printf("\nSystem Information:\n");
  printf("  CPU Count: %u\n", termux_cpu_count());

  uint32_t cpu_id = 0;
  if (termux_cpu_affinity_get(&cpu_id) == 0) {
    printf("  Current CPU: %u\n", cpu_id);
  }

  printf("\nPhase 9.7 Benchmark Configuration:\n");
  printf("  Total Layers: %u\n", MULTICORE_BENCHMARK_LAYERS);
  printf("  Packages per Layer: %u\n", MULTICORE_BENCHMARK_PACKAGES_PER_LAYER);
  printf("  Total Packages: %u\n", MULTICORE_BENCHMARK_LAYERS * MULTICORE_BENCHMARK_PACKAGES_PER_LAYER);
}

int main(void) {
  printf("\n");
  printf("================================================================================\n");
  printf("       TERMUX-PACKAGES MULTI-CORE PARALLELIZATION BENCHMARK (Phase 9.7)\n");
  printf("================================================================================\n");

  print_system_info();

  multicore_benchmark_result_t sequential = {};
  multicore_benchmark_result_t multicore_4 = {};
  multicore_benchmark_result_t multicore_8 = {};

  benchmark_sequential(&sequential);
  benchmark_multicore(4, &multicore_4);
  benchmark_multicore(8, &multicore_8);

  print_multicore_comparison(&sequential, &multicore_4);
  print_multicore_comparison(&sequential, &multicore_8);

  printf("Final Verdict:\n");
  printf("  4-way:  %.2fx speedup\n", (double)sequential.wall_time_ns / (double)multicore_4.wall_time_ns);
  printf("  8-way:  %.2fx speedup\n", (double)sequential.wall_time_ns / (double)multicore_8.wall_time_ns);
  printf("\n");

  return 0;
}
