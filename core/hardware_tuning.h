#ifndef TERMUX_HARDWARE_TUNING_H
#define TERMUX_HARDWARE_TUNING_H

#include <stdint.h>

#define TERMUX_MAX_GOLDEN_CORES 4
#define TERMUX_MAX_EFFICIENCY_CORES 4

enum termux_soc_type {
  TERMUX_SOC_UNKNOWN = 0,
  TERMUX_SOC_CORTEX_A53 = 1,    // ARM32 baseline
  TERMUX_SOC_CORTEX_A73 = 2,    // ARM64 high-performance
  TERMUX_SOC_SNAPDRAGON_888 = 3, // Qualcomm (1+3+4 cores)
  TERMUX_SOC_SNAPDRAGON_8GEN1 = 4, // Qualcomm Gen1 (1+2+5 cores)
  TERMUX_SOC_APPLE_M1 = 5,       // Apple M1 (4P + 4E cores)
  TERMUX_SOC_APPLE_M2 = 6,       // Apple M2 (4P + 4E cores)
  TERMUX_SOC_EXYNOS_9820 = 7,    // Samsung Exynos (2+2+4 cores)
  TERMUX_SOC_EXYNOS_2200 = 8,    // Samsung Exynos with GPU
};

struct termux_golden_core_config {
  uint32_t core_ids[TERMUX_MAX_GOLDEN_CORES];
  uint32_t count;
  uint32_t priority_boost;  // Frequency boost factor (0-100%)
};

struct termux_efficiency_core_config {
  uint32_t core_ids[TERMUX_MAX_EFFICIENCY_CORES];
  uint32_t count;
  uint32_t energy_limit;    // Power limit in mW
};

struct termux_hardware_config {
  enum termux_soc_type soc_type;
  uint32_t total_cores;
  uint32_t big_cores;       // Performance cores
  uint32_t little_cores;    // Efficiency cores
  struct termux_golden_core_config golden;
  struct termux_efficiency_core_config efficiency;
  uint32_t max_freq_mhz;
  uint32_t min_freq_mhz;
  uint8_t has_sve;          // Scalable Vector Extension
  uint8_t has_avx512;       // AVX-512 support
  uint8_t has_neon;         // ARM NEON support
  uint8_t _pad;
};

enum termux_soc_type termux_hardware_detect_soc(void);

int termux_hardware_get_config(struct termux_hardware_config *config);

int termux_hardware_enable_golden_cores(const struct termux_hardware_config *config);

int termux_hardware_enable_efficiency_cores(const struct termux_hardware_config *config);

uint32_t termux_hardware_get_frequency_boost(enum termux_soc_type soc);

const char *termux_hardware_soc_name(enum termux_soc_type soc);

#endif
