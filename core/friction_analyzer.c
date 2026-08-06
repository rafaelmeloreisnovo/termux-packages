#include "friction_analyzer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

static struct termux_friction_metrics global_metrics;

struct termux_friction_metrics* termux_get_metrics(void) {
  return &global_metrics;
}

void termux_metric_phase_start(const char *phase_name) {
  if (!phase_name) return;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  if (global_metrics.num_phases >= TERMUX_MAX_FRICTION_PHASES) {
    return;
  }

  struct termux_phase_metric *pm = &global_metrics.phases[global_metrics.num_phases];
  strncpy(pm->phase_name, phase_name, TERMUX_PHASE_NAME_LEN - 1);
  pm->phase_name[TERMUX_PHASE_NAME_LEN - 1] = '\0';
  pm->start_time = ts;
  pm->end_time = ts;
  pm->duration_ms = 0;
  pm->exit_code = 0;
  pm->active = 1;

  global_metrics.num_phases++;
}

void termux_metric_phase_end(const char *phase_name, int exit_code) {
  if (!phase_name) return;

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  for (size_t i = 0; i < global_metrics.num_phases; i++) {
    struct termux_phase_metric *pm = &global_metrics.phases[i];
    if (pm->active && strcmp(pm->phase_name, phase_name) == 0) {
      pm->end_time = ts;
      pm->exit_code = exit_code;
      pm->active = 0;

      uint64_t sec_diff = ts.tv_sec - pm->start_time.tv_sec;
      int64_t nsec_diff = ts.tv_nsec - pm->start_time.tv_nsec;
      if (nsec_diff < 0) {
        sec_diff--;
        nsec_diff += 1000000000;
      }
      pm->duration_ms = (uint64_t)sec_diff * 1000 + (uint64_t)nsec_diff / 1000000;
      break;
    }
  }
}

int termux_write_metrics_report(const char *output_path) {
  if (!output_path) return -1;

  int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return -1;
  }

  char buf[512];
  int n;

  n = snprintf(buf, sizeof(buf), "=== FRICTION ANALYSIS REPORT ===\n");
  if (write(fd, buf, n) != n) {
    close(fd);
    return -1;
  }

  n = snprintf(buf, sizeof(buf), "Total phases: %u\n\n", global_metrics.num_phases);
  if (write(fd, buf, n) != n) {
    close(fd);
    return -1;
  }

  uint64_t total_duration = 0;

  for (size_t i = 0; i < global_metrics.num_phases; i++) {
    const struct termux_phase_metric *pm = &global_metrics.phases[i];
    total_duration += pm->duration_ms;

    n = snprintf(buf, sizeof(buf), "[%2zu] %-20s %10" PRIu64 " ms (exit: %d)\n",
                 i + 1, pm->phase_name, pm->duration_ms, pm->exit_code);
    if (write(fd, buf, n) != n) {
      close(fd);
      return -1;
    }
  }

  n = snprintf(buf, sizeof(buf), "\nTotal time: %" PRIu64 " ms (%.2f s)\n",
               total_duration, (double)total_duration / 1000.0);
  if (write(fd, buf, n) != n) {
    close(fd);
    return -1;
  }

  if (global_metrics.num_phases > 0) {
    uint64_t avg_duration = total_duration / global_metrics.num_phases;
    n = snprintf(buf, sizeof(buf), "Average phase time: %" PRIu64 " ms\n", avg_duration);
    if (write(fd, buf, n) != n) {
      close(fd);
      return -1;
    }
  }

  n = snprintf(buf, sizeof(buf), "\n=== SLOWEST PHASES ===\n");
  if (write(fd, buf, n) != n) {
    close(fd);
    return -1;
  }

  for (size_t rank = 0; rank < 3 && rank < global_metrics.num_phases; rank++) {
    uint64_t max_duration = 0;
    size_t max_idx = 0;

    for (size_t i = 0; i < global_metrics.num_phases; i++) {
      const struct termux_phase_metric *pm = &global_metrics.phases[i];
      if (!pm->analyzed && pm->duration_ms > max_duration) {
        max_duration = pm->duration_ms;
        max_idx = i;
      }
    }

    if (max_duration > 0) {
      struct termux_phase_metric *pm = &global_metrics.phases[max_idx];
      pm->analyzed = 1;

      n = snprintf(buf, sizeof(buf), "%u. %s: %" PRIu64 " ms\n",
                   (unsigned)(rank + 1), pm->phase_name, pm->duration_ms);
      if (write(fd, buf, n) != n) {
        close(fd);
        return -1;
      }
    }
  }

  close(fd);
  return 0;
}

void termux_print_metrics_summary(void) {
  printf("\n=== FRICTION METRICS ===\n");
  printf("Phases executed: %u\n", global_metrics.num_phases);

  uint64_t total_ms = 0;
  for (size_t i = 0; i < global_metrics.num_phases; i++) {
    const struct termux_phase_metric *pm = &global_metrics.phases[i];
    total_ms += pm->duration_ms;
    printf("  %s: %" PRIu64 " ms\n", pm->phase_name, pm->duration_ms);
  }

  printf("Total time: %" PRIu64 " ms (%.2f s)\n", total_ms, (double)total_ms / 1000.0);
  printf("======================\n\n");
}

void termux_reset_metrics(void) {
  memset(&global_metrics, 0, sizeof(global_metrics));
}
