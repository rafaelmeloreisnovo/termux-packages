#ifndef TERMUX_ADAPTIVE_DVFS_H
#define TERMUX_ADAPTIVE_DVFS_H

#include <stdint.h>
#include <stdbool.h>

#define TERMUX_MAX_FREQUENCY_DOMAINS 8
#define TERMUX_MAX_FREQUENCY_LEVELS 5

typedef enum {
  DVFS_LEVEL_MINIMAL = 0,    // 800 MHz (minimum)
  DVFS_LEVEL_LOW = 1,        // 1200 MHz
  DVFS_LEVEL_MEDIUM = 2,     // 1800 MHz
  DVFS_LEVEL_HIGH = 3,       // 2400 MHz
  DVFS_LEVEL_MAXIMUM = 4     // 3200 MHz (maximum boost)
} dvfs_level_t;

typedef struct {
  uint32_t freq_mhz;
  uint32_t power_mw;
  double latency_factor;      // 1.0 = baseline
  double energy_per_cycle;
} frequency_point_t;

typedef struct {
  uint32_t domain_id;
  dvfs_level_t current_level;
  uint32_t current_freq_mhz;
  uint64_t total_cycles;
  uint64_t total_energy_uj;   // micro-joules
  double coherence_phi;
  uint32_t active_cores;
} frequency_domain_t;

typedef struct {
  frequency_domain_t domains[TERMUX_MAX_FREQUENCY_DOMAINS];
  uint32_t domain_count;
  uint64_t total_energy_uj;

  // DVFS control thresholds
  double phi_high_threshold;  // Scale down if φ > this
  double phi_low_threshold;   // Scale up if φ < this
  uint32_t scaling_interval;  // milliseconds between checks

  // Frequency tables per domain
  frequency_point_t freq_points[TERMUX_MAX_FREQUENCY_LEVELS];
  uint32_t freq_point_count;

  // Statistics
  uint64_t scaling_events;
  uint64_t energy_saved_uj;
} adaptive_dvfs_t;

int termux_adaptive_dvfs_init(adaptive_dvfs_t *dvfs, uint32_t domain_count);

void termux_adaptive_dvfs_destroy(adaptive_dvfs_t *dvfs);

int termux_dvfs_set_frequency(adaptive_dvfs_t *dvfs, uint32_t domain_id, dvfs_level_t level);

dvfs_level_t termux_dvfs_get_frequency(const adaptive_dvfs_t *dvfs, uint32_t domain_id);

int termux_dvfs_adjust_for_coherence(adaptive_dvfs_t *dvfs, uint32_t domain_id, double coherence_phi);

int termux_dvfs_set_thresholds(adaptive_dvfs_t *dvfs, double high, double low);

uint64_t termux_dvfs_estimate_energy(adaptive_dvfs_t *dvfs, uint32_t domain_id, uint64_t cycles);

double termux_dvfs_efficiency_metric(const adaptive_dvfs_t *dvfs);

void termux_dvfs_print_stats(const adaptive_dvfs_t *dvfs);

#endif
