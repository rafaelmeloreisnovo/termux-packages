#include "adaptive_dvfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

static const frequency_point_t default_freq_points[TERMUX_MAX_FREQUENCY_LEVELS] = {
  {.freq_mhz = 800,  .power_mw = 500,  .latency_factor = 2.5, .energy_per_cycle = 0.625},
  {.freq_mhz = 1200, .power_mw = 800,  .latency_factor = 1.8, .energy_per_cycle = 0.667},
  {.freq_mhz = 1800, .power_mw = 1400, .latency_factor = 1.2, .energy_per_cycle = 0.778},
  {.freq_mhz = 2400, .power_mw = 2200, .latency_factor = 1.0, .energy_per_cycle = 0.917},
  {.freq_mhz = 3200, .power_mw = 3500, .latency_factor = 0.8, .energy_per_cycle = 1.094}
};

int termux_adaptive_dvfs_init(adaptive_dvfs_t *dvfs, uint32_t domain_count) {
  if (!dvfs || domain_count == 0 || domain_count > TERMUX_MAX_FREQUENCY_DOMAINS) {
    return -1;
  }

  memset(dvfs, 0, sizeof(*dvfs));
  dvfs->domain_count = domain_count;

  for (uint32_t i = 0; i < domain_count; i++) {
    dvfs->domains[i].domain_id = i;
    dvfs->domains[i].current_level = DVFS_LEVEL_MEDIUM;
    dvfs->domains[i].current_freq_mhz = default_freq_points[DVFS_LEVEL_MEDIUM].freq_mhz;
    dvfs->domains[i].coherence_phi = 1.0;
    dvfs->domains[i].active_cores = 1;
  }

  memcpy(dvfs->freq_points, default_freq_points, sizeof(default_freq_points));
  dvfs->freq_point_count = TERMUX_MAX_FREQUENCY_LEVELS;

  dvfs->phi_high_threshold = 0.95;
  dvfs->phi_low_threshold = 0.75;
  dvfs->scaling_interval = 100;

  return 0;
}

void termux_adaptive_dvfs_destroy(adaptive_dvfs_t *dvfs) {
  if (!dvfs) return;
  memset(dvfs, 0, sizeof(*dvfs));
}

int termux_dvfs_set_frequency(adaptive_dvfs_t *dvfs, uint32_t domain_id, dvfs_level_t level) {
  if (!dvfs || domain_id >= dvfs->domain_count || level >= TERMUX_MAX_FREQUENCY_LEVELS) {
    return -1;
  }

  frequency_domain_t *domain = &dvfs->domains[domain_id];
  dvfs_level_t old_level = domain->current_level;

  domain->current_level = level;
  domain->current_freq_mhz = dvfs->freq_points[level].freq_mhz;

  if (old_level != level) {
    dvfs->scaling_events++;
  }

  return 0;
}

dvfs_level_t termux_dvfs_get_frequency(const adaptive_dvfs_t *dvfs, uint32_t domain_id) {
  if (!dvfs || domain_id >= dvfs->domain_count) {
    return DVFS_LEVEL_MEDIUM;
  }
  return dvfs->domains[domain_id].current_level;
}

int termux_dvfs_adjust_for_coherence(adaptive_dvfs_t *dvfs, uint32_t domain_id, double coherence_phi) {
  if (!dvfs || domain_id >= dvfs->domain_count || coherence_phi < 0.0 || coherence_phi > 1.0) {
    return -1;
  }

  frequency_domain_t *domain = &dvfs->domains[domain_id];
  domain->coherence_phi = coherence_phi;

  dvfs_level_t new_level = domain->current_level;

  if (coherence_phi > dvfs->phi_high_threshold) {
    if (new_level > DVFS_LEVEL_MINIMAL) {
      new_level = (dvfs_level_t)(new_level - 1);
    }
  } else if (coherence_phi < dvfs->phi_low_threshold) {
    if (new_level < DVFS_LEVEL_MAXIMUM) {
      new_level = (dvfs_level_t)(new_level + 1);
    }
  }

  return termux_dvfs_set_frequency(dvfs, domain_id, new_level);
}

int termux_dvfs_set_thresholds(adaptive_dvfs_t *dvfs, double high, double low) {
  if (!dvfs || high <= low || high > 1.0 || low < 0.0) {
    return -1;
  }

  dvfs->phi_high_threshold = high;
  dvfs->phi_low_threshold = low;

  return 0;
}

uint64_t termux_dvfs_estimate_energy(adaptive_dvfs_t *dvfs, uint32_t domain_id, uint64_t cycles) {
  if (!dvfs || domain_id >= dvfs->domain_count || cycles == 0) {
    return 0;
  }

  frequency_domain_t *domain = &dvfs->domains[domain_id];
  dvfs_level_t level = domain->current_level;

  double power_mw = dvfs->freq_points[level].power_mw;
  uint32_t freq_mhz = dvfs->freq_points[level].freq_mhz;

  uint64_t time_us = (cycles * 1000) / freq_mhz;
  uint64_t energy_uj = (power_mw * time_us) / 1000;

  domain->total_energy_uj += energy_uj;
  domain->total_cycles += cycles;
  dvfs->total_energy_uj += energy_uj;

  return energy_uj;
}

double termux_dvfs_efficiency_metric(const adaptive_dvfs_t *dvfs) {
  if (!dvfs) return 0.0;

  uint64_t total_cycles = 0;
  for (uint32_t i = 0; i < dvfs->domain_count; i++) {
    total_cycles += dvfs->domains[i].total_cycles;
  }

  if (total_cycles == 0 || dvfs->total_energy_uj == 0) {
    return 0.0;
  }

  double avg_coherence = 0.0;
  for (uint32_t i = 0; i < dvfs->domain_count; i++) {
    avg_coherence += dvfs->domains[i].coherence_phi;
  }
  avg_coherence /= dvfs->domain_count;

  double energy_efficiency = (double)total_cycles / (double)dvfs->total_energy_uj;

  return avg_coherence * energy_efficiency * 0.001;
}

void termux_dvfs_print_stats(const adaptive_dvfs_t *dvfs) {
  if (!dvfs) return;

  printf("\n");
  printf("================================================================================\n");
  printf("                     ADAPTIVE DVFS STATISTICS\n");
  printf("================================================================================\n");
  printf("Frequency Domains: %u\n", dvfs->domain_count);
  printf("Total Energy Consumed: %" PRIu64 " µJ\n", dvfs->total_energy_uj);
  printf("Scaling Events: %" PRIu64 "\n", dvfs->scaling_events);
  printf("Energy Saved: %" PRIu64 " µJ\n", dvfs->energy_saved_uj);

  printf("\nPer-Domain Metrics:\n");
  for (uint32_t i = 0; i < dvfs->domain_count; i++) {
    const frequency_domain_t *domain = &dvfs->domains[i];
    printf("  Domain %u:\n", domain->domain_id);
    printf("    Current Frequency: %u MHz\n", domain->current_freq_mhz);
    printf("    Coherence φ: %.4f\n", domain->coherence_phi);
    printf("    Total Cycles: %" PRIu64 "\n", domain->total_cycles);
    printf("    Total Energy: %" PRIu64 " µJ\n", domain->total_energy_uj);
    printf("    Active Cores: %u\n", domain->active_cores);
  }

  printf("\nDVFS Thresholds:\n");
  printf("  High Threshold (scale down): %.2f\n", dvfs->phi_high_threshold);
  printf("  Low Threshold (scale up): %.2f\n", dvfs->phi_low_threshold);

  printf("\nEfficiency Metric: %.4f\n", termux_dvfs_efficiency_metric(dvfs));
  printf("================================================================================\n\n");
}
