#include "build_orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#define PERF_MAX_SAMPLES 2057
#define PERF_HISTOGRAM_BUCKETS 10

typedef struct {
  uint32_t pkg_idx;
  char pkg_name[64];
  uint64_t wall_time_ns;
  uint32_t cycle_count;
  uint64_t coherence_phi;
  uint32_t phase;
  uint32_t arch_state;
} perf_sample_t;

typedef struct {
  perf_sample_t samples[PERF_MAX_SAMPLES];
  uint32_t sample_count;
  uint64_t total_wall_time_ns;
  uint64_t total_phi;
  uint32_t max_cycle_count;
  uint32_t min_cycle_count;
} perf_profile_t;

static perf_profile_t profile = {};

static inline uint64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int compare_u64(const void *lhs, const void *rhs) {
  const uint64_t a = *(const uint64_t *)lhs;
  const uint64_t b = *(const uint64_t *)rhs;
  return (a > b) - (a < b);
}

static void perf_record_sample(uint32_t pkg_idx, const char *pkg_name,
                               uint64_t wall_time_ns,
                               struct termux_build_state *state) {
  if (profile.sample_count >= PERF_MAX_SAMPLES) return;

  perf_sample_t *s = &profile.samples[profile.sample_count++];
  s->pkg_idx = pkg_idx;
  strncpy(s->pkg_name, pkg_name, sizeof(s->pkg_name) - 1);
  s->wall_time_ns = wall_time_ns;
  s->cycle_count = state->cycle_count;
  s->coherence_phi = state->coherence_phi;
  s->phase = state->phase;
  s->arch_state = state->arch_state;

  profile.total_wall_time_ns += wall_time_ns;
  profile.total_phi += state->coherence_phi;

  if (profile.sample_count == 1) {
    profile.max_cycle_count = state->cycle_count;
    profile.min_cycle_count = state->cycle_count;
  } else {
    if (state->cycle_count > profile.max_cycle_count) {
      profile.max_cycle_count = state->cycle_count;
    }
    if (state->cycle_count < profile.min_cycle_count) {
      profile.min_cycle_count = state->cycle_count;
    }
  }
}

static int perf_profile_build_cycle(uint32_t num_packages) {
  printf("\n=== Performance Profiling (Sampling %u packages) ===\n\n", num_packages);

  struct termux_orchestrator orch = {};
  int ret = termux_orchestrator_init(&orch);
  if (ret != 0) {
    printf("✗ Orchestrator init failed: %d\n", ret);
    return -1;
  }

  uint32_t packages_to_profile = num_packages > PERF_MAX_SAMPLES ? PERF_MAX_SAMPLES : num_packages;

  for (uint32_t pkg = 0; pkg < packages_to_profile; pkg++) {
    char pkg_name[64] = {};
    snprintf(pkg_name, sizeof(pkg_name), "pkg-%u", pkg);

    uint64_t start_ns = get_time_ns();
    ret = termux_orchestrator_execute(&orch, pkg_name, pkg);
    uint64_t end_ns = get_time_ns();

    if (ret != 0) {
      printf("✗ Package %u execution failed: %d\n", pkg, ret);
      continue;
    }

    uint64_t elapsed_ns = end_ns - start_ns;
    perf_record_sample(pkg, pkg_name, elapsed_ns, &orch.state);

    if ((pkg + 1) % 256 == 0) {
      printf("  Progress: %u/%u packages profiled\n", pkg + 1, packages_to_profile);
    }
  }

  return 0;
}

static void perf_print_statistics(void) {
  if (profile.sample_count == 0) {
    printf("✗ No samples collected\n");
    return;
  }

  printf("\n=== Performance Statistics ===\n\n");

  double avg_wall_time_ms = (double)profile.total_wall_time_ns / (double)profile.sample_count / 1000000.0;
  double avg_phi = (double)profile.total_phi / (double)profile.sample_count;
  double normalized_phi = avg_phi / (double)(1ULL << 20);

  printf("Sample Count: %u\n", profile.sample_count);
  printf("Total Wall Time: %.2f seconds\n", (double)profile.total_wall_time_ns / 1e9);
  printf("Average Wall Time per Package: %.2f ms\n", avg_wall_time_ms);
  printf("\nCycle Count Statistics:\n");
  printf("  Min: %u cycles\n", profile.min_cycle_count);
  printf("  Max: %u cycles\n", profile.max_cycle_count);
  printf("  Target: ≤ 42 cycles per COLLAPSE_STEP\n");

  printf("\nCoherence Metric Φ:\n");
  printf("  Average Φ: %.4f (normalized to [0,1] scale)\n", normalized_phi);
  printf("  Target: Φ > 0.85\n");

  uint32_t phi_violations = 0;
  for (uint32_t i = 0; i < profile.sample_count; i++) {
    if (profile.samples[i].coherence_phi < ((1ULL << 20) * 8 / 10)) {
      phi_violations++;
    }
  }
  printf("  Violations (Φ < 0.80): %u/%.u (%.1f%%)\n",
         phi_violations, profile.sample_count,
         (double)phi_violations / (double)profile.sample_count * 100.0);

  printf("\nPhase Distribution:\n");
  uint32_t phase_counts[7] = {};
  for (uint32_t i = 0; i < profile.sample_count; i++) {
    if (profile.samples[i].phase < 7) {
      phase_counts[profile.samples[i].phase]++;
    }
  }
  for (uint32_t p = 0; p < 7; p++) {
    printf("  Phase %u: %u packages (%.1f%%)\n", p, phase_counts[p],
           (double)phase_counts[p] / (double)profile.sample_count * 100.0);
  }

  printf("\nArchitecture State Distribution:\n");
  uint32_t arch_counts[6] = {};
  for (uint32_t i = 0; i < profile.sample_count; i++) {
    if (profile.samples[i].arch_state < 6) {
      arch_counts[profile.samples[i].arch_state]++;
    }
  }
  for (uint32_t a = 0; a < 6; a++) {
    printf("  Arch%u: %u packages (%.1f%%)\n", a, arch_counts[a],
           (double)arch_counts[a] / (double)profile.sample_count * 100.0);
  }
}

static void perf_print_wall_time_histogram(void) {
  printf("\n=== Wall Time Distribution ===\n\n");

  if (profile.sample_count < 2) {
    printf("  (insufficient samples)\n");
    return;
  }

  uint64_t min_time = UINT64_MAX, max_time = 0;
  for (uint32_t i = 0; i < profile.sample_count; i++) {
    if (profile.samples[i].wall_time_ns < min_time) {
      min_time = profile.samples[i].wall_time_ns;
    }
    if (profile.samples[i].wall_time_ns > max_time) {
      max_time = profile.samples[i].wall_time_ns;
    }
  }

  uint64_t range = max_time - min_time;
  uint64_t bucket_size = range / PERF_HISTOGRAM_BUCKETS + 1;
  uint32_t histogram[PERF_HISTOGRAM_BUCKETS] = {};

  for (uint32_t i = 0; i < profile.sample_count; i++) {
    uint32_t bucket = (profile.samples[i].wall_time_ns - min_time) / bucket_size;
    if (bucket >= PERF_HISTOGRAM_BUCKETS) bucket = PERF_HISTOGRAM_BUCKETS - 1;
    histogram[bucket]++;
  }

  for (uint32_t b = 0; b < PERF_HISTOGRAM_BUCKETS; b++) {
    uint64_t bucket_start = min_time + (b * bucket_size);
    uint64_t bucket_end = bucket_start + bucket_size;
    uint32_t count = histogram[b];
    double percent = (double)count / (double)profile.sample_count * 100.0;

    printf("  [%3" PRIu64 "-%3" PRIu64 ") ms: ",
           bucket_start / 1000000, bucket_end / 1000000);
    for (uint32_t i = 0; i < count && i < 40; i++) printf("█");
    printf(" %u (%.1f%%)\n", count, percent);
  }

  printf("\n  Min: %.2f ms\n", (double)min_time / 1e6);
  printf("  Max: %.2f ms\n", (double)max_time / 1e6);
  printf("  Target (ARM32): < 5000 ms (5 min)\n");
  printf("  Target (ARM64): < 3000 ms (3 min)\n");
}

static void perf_identify_outliers(void) {
  printf("\n=== Outlier Analysis ===\n\n");

  if (profile.sample_count < 10) {
    printf("  (insufficient samples for outlier detection)\n");
    return;
  }

  uint64_t *times = malloc(profile.sample_count * sizeof(uint64_t));
  if (!times) return;

  for (uint32_t i = 0; i < profile.sample_count; i++) {
    times[i] = profile.samples[i].wall_time_ns;
  }

  qsort(times, profile.sample_count, sizeof(uint64_t), compare_u64);

  uint32_t q1_idx = profile.sample_count / 4;
  uint32_t q3_idx = (profile.sample_count * 3) / 4;
  uint64_t q1 = times[q1_idx];
  uint64_t q3 = times[q3_idx];
  uint64_t iqr = q3 - q1;
  uint64_t upper_bound = q3 + (iqr * 3 / 2);

  uint32_t outlier_count = 0;
  printf("Packages exceeding 1.5×IQR threshold:\n");
  for (uint32_t i = 0; i < profile.sample_count; i++) {
    if (profile.samples[i].wall_time_ns > upper_bound) {
      outlier_count++;
      if (outlier_count <= 10) {
        printf("  %s: %.2f ms (threshold: %.2f ms)\n",
               profile.samples[i].pkg_name,
               (double)profile.samples[i].wall_time_ns / 1e6,
               (double)upper_bound / 1e6);
      }
    }
  }
  if (outlier_count > 10) {
    printf("  ... and %u more\n", outlier_count - 10);
  }
  printf("\nTotal outliers: %u/%.u (%.1f%%)\n", outlier_count, profile.sample_count,
         (double)outlier_count / (double)profile.sample_count * 100.0);

  free(times);
}

int main(int argc, char *argv[]) {
  uint32_t num_packages = 256;

  if (argc > 1) {
    num_packages = atoi(argv[1]);
    if (num_packages > PERF_MAX_SAMPLES) num_packages = PERF_MAX_SAMPLES;
    if (num_packages < 1) num_packages = 1;
  }

  printf("\n");
  printf("================================================================================\n");
  printf("              TERMUX-PACKAGES PERFORMANCE PROFILER (Phase 9.4)\n");
  printf("================================================================================\n");

  int ret = perf_profile_build_cycle(num_packages);
  if (ret != 0) {
    printf("\n✗ Profiling failed\n");
    return 1;
  }

  perf_print_statistics();
  perf_print_wall_time_histogram();
  perf_identify_outliers();

  printf("\n================================================================================\n");
  printf("Coherence Metric φ Assessment:\n");
  double avg_phi = (double)profile.total_phi / (double)profile.sample_count / (double)(1ULL << 20);
  if (avg_phi > 0.85) {
    printf("  ✓ φ = %.4f > 0.85 TARGET ACHIEVED\n", avg_phi);
  } else if (avg_phi > 0.75) {
    printf("  ⚠ φ = %.4f (good, but below 0.85 target)\n", avg_phi);
  } else {
    printf("  ✗ φ = %.4f (below expectations, investigate cache/latency)\n", avg_phi);
  }
  printf("================================================================================\n\n");

  return 0;
}
