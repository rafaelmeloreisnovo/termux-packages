#include "profiler.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

int profiler_session_init(profiler_session_t *session, const char *build_id) {
  if (!session || !build_id) return -1;

  memset(session, 0, sizeof(*session));
  strncpy(session->build_id, build_id, sizeof(session->build_id) - 1);

  return 0;
}

int profiler_session_start(profiler_session_t *session) {
  if (!session) return -1;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  session->timestamp_start = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

  memset(&session->cycle_prof, 0, sizeof(session->cycle_prof));
  termux_cache_metrics_measure(&session->cache_metrics);

  return 0;
}

int profiler_session_record_package(profiler_session_t *session,
                                   uint32_t _pkg_idx,
                                   const cycle_profile_t *cycles) {
  if (!session || !cycles) return -1;
  (void)_pkg_idx;

  session->packages_completed++;
  session->cycle_prof.total_cycles += cycles->total_cycles;
  session->cycle_prof.violations += cycles->violations;

  if (cycles->peak_cycles > session->cycle_prof.peak_cycles) {
    session->cycle_prof.peak_cycles = cycles->peak_cycles;
  }

  return 0;
}

int profiler_session_finalize(profiler_session_t *session) {
  if (!session) return -1;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t timestamp_end = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

  uint64_t elapsed = timestamp_end > session->timestamp_start ?
    timestamp_end - session->timestamp_start : 1000000000ULL;

  session->wall_time_sec = (double)elapsed / 1.0e9;
  if (session->wall_time_sec < 0.001) session->wall_time_sec = 0.001;

  if (session->packages_completed > 0 && session->wall_time_sec > 0.0) {
    session->throughput_pkgs_per_sec = (double)session->packages_completed / session->wall_time_sec;
  } else {
    session->throughput_pkgs_per_sec = 0.0;
  }

  double baseline_throughput = 10.0;
  session->speedup_vs_baseline = session->throughput_pkgs_per_sec / baseline_throughput;

  termux_cycle_efficiency_compute(&session->cycle_prof);

  for (uint32_t i = 0; i < TERMUX_DAG_LAYERS; i++) {
    session->coherence_layers[i].layer_id = i;
    termux_layer_coherence_optimize(&session->coherence_layers[i],
                                   &session->cache_metrics);
  }

  return 0;
}

void profiler_session_print_summary(const profiler_session_t *session) {
  if (!session) return;

  printf("\n");
  printf("================================================================================\n");
  printf("                     BUILD PROFILER SESSION SUMMARY\n");
  printf("================================================================================\n");
  printf("Build ID: %s\n", session->build_id);
  printf("Wall time: %.2f seconds\n", session->wall_time_sec);
  printf("Packages completed: %u / %u\n", session->packages_completed, session->total_packages);
  printf("Packages failed: %u\n", session->packages_failed);

  printf("\nPerformance Metrics:\n");
  printf("  Throughput: %.2f packages/sec\n", session->throughput_pkgs_per_sec);
  printf("  Speedup vs baseline: %.2fx\n", session->speedup_vs_baseline);
  printf("  Total cycles: %u / %u (%.1f%% budget)\n",
         session->cycle_prof.total_cycles,
         TERMUX_MAX_CYCLES_PER_PACKAGE * session->packages_completed,
         (double)session->cycle_prof.total_cycles /
         (double)(TERMUX_MAX_CYCLES_PER_PACKAGE * session->packages_completed) * 100.0);

  printf("\nCache Performance:\n");
  printf("  L1 miss rate: %.2f%%\n", session->cache_metrics.l1_miss_rate * 100.0);
  printf("  L2 miss rate: %.2f%%\n", session->cache_metrics.l2_miss_rate * 100.0);
  printf("  Cache locality score: %.2f%%\n",
         termux_cache_locality_score(&session->cache_metrics) * 100.0);

  printf("\nCoherence Analysis:\n");
  uint64_t total_phi = 0;
  for (uint32_t i = 0; i < TERMUX_DAG_LAYERS; i++) {
    total_phi += session->coherence_layers[i].coherence_phi;
  }
  double avg_phi = total_phi / (double)TERMUX_DAG_LAYERS;
  printf("  Mean Φ: %.2f\n", avg_phi);
  printf("  Target: Φ > 0.85 (coherence factor)\n");

  printf("================================================================================\n\n");
}

void profiler_session_print_timeline(const profiler_session_t *session) {
  if (!session) return;

  printf("\n=== Cycle Budget Timeline ===\n");
  printf("Per-phase breakdown (first package):\n");

  for (int i = 0; i < 8; i++) {
    const cycle_measurement_t *m = &session->cycle_prof.measurements[i];
    printf("  Phase %d: %u cycles / %u budget %s\n",
           i, m->cycles_actual, m->cycles_budget,
           m->exceeded ? "✗ EXCEEDED" : "✓ OK");
  }

  printf("\nLayers & Coherence:\n");
  for (uint32_t i = 0; i < TERMUX_DAG_LAYERS; i++) {
    const layer_coherence_t *l = &session->coherence_layers[i];
    printf("  Layer %2u: Φ=%6" PRIu64 ", Cache=%.0f%%, Mem=%.0f%%\n",
           i, l->coherence_phi,
           l->cache_locality * 100.0,
           l->memory_efficiency * 100.0);
  }

  printf("\n");
}

void profiler_session_export_json(const profiler_session_t *session,
                                 const char *filename) {
  if (!session || !filename) return;

  FILE *f = fopen(filename, "w");
  if (!f) return;

  fprintf(f, "{\n");
  fprintf(f, "  \"build_id\": \"%s\",\n", session->build_id);
  fprintf(f, "  \"wall_time_sec\": %.2f,\n", session->wall_time_sec);
  fprintf(f, "  \"packages_completed\": %u,\n", session->packages_completed);
  fprintf(f, "  \"throughput_pkgs_per_sec\": %.2f,\n", session->throughput_pkgs_per_sec);
  fprintf(f, "  \"speedup_vs_baseline\": %.2f,\n", session->speedup_vs_baseline);
  fprintf(f, "  \"cycle_profile\": {\n");
  fprintf(f, "    \"total_cycles\": %u,\n", session->cycle_prof.total_cycles);
  fprintf(f, "    \"peak_cycles\": %u,\n", session->cycle_prof.peak_cycles);
  fprintf(f, "    \"violations\": %u\n", session->cycle_prof.violations);
  fprintf(f, "  },\n");
  fprintf(f, "  \"cache_metrics\": {\n");
  fprintf(f, "    \"l1_miss_rate\": %.4f,\n", session->cache_metrics.l1_miss_rate);
  fprintf(f, "    \"l2_miss_rate\": %.4f\n", session->cache_metrics.l2_miss_rate);
  fprintf(f, "  }\n");
  fprintf(f, "}\n");

  fclose(f);
}
