#ifndef TERMUX_HARDWARE_TUNING_H
#define TERMUX_HARDWARE_TUNING_H

#include <stdint.h>
#include <stdbool.h>

#define TERMUX_MAX_GOLDEN_CORES 4
#define TERMUX_MAX_EFFICIENCY_CORES 4
#define TERMUX_MAX_CORES 8

enum termux_soc_type {
  TERMUX_SOC_UNKNOWN = 0,
  TERMUX_SOC_CORTEX_A53 = 1,
  TERMUX_SOC_CORTEX_A73 = 2,
  TERMUX_SOC_SNAPDRAGON_888 = 3,
  TERMUX_SOC_SNAPDRAGON_8GEN1 = 4,
  TERMUX_SOC_APPLE_M1 = 5,
  TERMUX_SOC_APPLE_M2 = 6,
  TERMUX_SOC_EXYNOS_9820 = 7,
  TERMUX_SOC_EXYNOS_2200 = 8,
};

struct termux_golden_core_config {
  uint32_t core_ids[TERMUX_MAX_GOLDEN_CORES];
  uint32_t count;
  uint32_t priority_boost;
};

struct termux_efficiency_core_config {
  uint32_t core_ids[TERMUX_MAX_EFFICIENCY_CORES];
  uint32_t count;
  uint32_t energy_limit;
};

struct termux_hardware_config {
  enum termux_soc_type soc_type;
  uint32_t total_cores;
  uint32_t big_cores;
  uint32_t little_cores;
  struct termux_golden_core_config golden;
  struct termux_efficiency_core_config efficiency;
  uint32_t max_freq_mhz;
  uint32_t min_freq_mhz;
  uint8_t has_sve;
  uint8_t has_avx512;
  uint8_t has_neon;
  uint8_t _pad;
};

typedef struct {
  enum termux_soc_type soc_type;
  uint32_t core_count;
  uint32_t golden_core_id;
  uint32_t perf_core_mask;
  uint32_t effi_core_mask;
  double performance_scaling;
  double efficiency_scaling;
  uint32_t l3_cache_kb;
  bool has_neon;
  bool has_sve;
  bool has_dotprod;
  uint32_t max_freq_mhz;
  uint32_t min_freq_mhz;
} platform_profile_t;

typedef struct {
  platform_profile_t profile;
  uint32_t core_frequencies[8];
  uint8_t core_types[8];
  uint64_t core_utilization[8];
  double coherence_scores[8];
  uint32_t scheduled_packages[256];
  uint32_t scheduled_count;
} hardware_context_t;

enum termux_soc_type termux_hardware_detect_soc(void);

int termux_hardware_get_config(struct termux_hardware_config *config);

int termux_hardware_enable_golden_cores(const struct termux_hardware_config *config);

int termux_hardware_enable_efficiency_cores(const struct termux_hardware_config *config);

uint32_t termux_hardware_get_frequency_boost(enum termux_soc_type soc);

const char *termux_hardware_soc_name(enum termux_soc_type soc);

int termux_platform_profile_init(hardware_context_t *ctx, enum termux_soc_type soc);

int termux_golden_core_prioritize(hardware_context_t *ctx, uint32_t pkg_idx);

int termux_efficiency_core_bypass(hardware_context_t *ctx, uint32_t pkg_idx);

int termux_big_little_load_balance(hardware_context_t *ctx, uint32_t layer_idx);

int termux_neon_mapping_optimize(hardware_context_t *ctx);

double termux_coherence_score_platform(const hardware_context_t *ctx, uint32_t core_id);

void termux_platform_print_profile(const hardware_context_t *ctx);

#endif
