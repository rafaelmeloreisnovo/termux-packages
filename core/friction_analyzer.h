#ifndef TERMUX_FRICTION_ANALYZER_H
#define TERMUX_FRICTION_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <inttypes.h>

#define TERMUX_MAX_FRICTION_PHASES 16
#define TERMUX_PHASE_NAME_LEN 32

struct termux_phase_metric {
  char phase_name[TERMUX_PHASE_NAME_LEN];
  struct timespec start_time;
  struct timespec end_time;
  uint64_t duration_ms;
  int exit_code;
  uint8_t active;
  uint8_t analyzed;
};

struct termux_friction_metrics {
  struct termux_phase_metric phases[TERMUX_MAX_FRICTION_PHASES];
  uint32_t num_phases;
};

struct termux_friction_metrics* termux_get_metrics(void);
void termux_metric_phase_start(const char *phase_name);
void termux_metric_phase_end(const char *phase_name, int exit_code);
int termux_write_metrics_report(const char *output_path);
void termux_print_metrics_summary(void);
void termux_reset_metrics(void);

#endif
