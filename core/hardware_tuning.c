#include "hardware_tuning.h"
#include "cpu_affinity.h"
#include <string.h>
#include <stdio.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

enum termux_soc_type termux_hardware_detect_soc(void) {
  FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
  if (!cpuinfo) {
    return TERMUX_SOC_UNKNOWN;
  }

  char line[256];
  enum termux_soc_type detected = TERMUX_SOC_UNKNOWN;

  while (fgets(line, sizeof(line), cpuinfo)) {
    if (strstr(line, "Snapdragon 8 Gen 1")) {
      detected = TERMUX_SOC_SNAPDRAGON_8GEN1;
      break;
    } else if (strstr(line, "Snapdragon 888")) {
      detected = TERMUX_SOC_SNAPDRAGON_888;
      break;
    } else if (strstr(line, "Apple M1")) {
      detected = TERMUX_SOC_APPLE_M1;
      break;
    } else if (strstr(line, "Apple M2")) {
      detected = TERMUX_SOC_APPLE_M2;
      break;
    } else if (strstr(line, "Exynos 2200")) {
      detected = TERMUX_SOC_EXYNOS_2200;
      break;
    } else if (strstr(line, "Exynos 9820")) {
      detected = TERMUX_SOC_EXYNOS_9820;
      break;
    } else if (strstr(line, "ARMv8") || strstr(line, "ARMv7")) {
      if (strstr(line, "Cortex-A73")) {
        detected = TERMUX_SOC_CORTEX_A73;
      } else if (strstr(line, "Cortex-A53")) {
        detected = TERMUX_SOC_CORTEX_A53;
      }
    }
  }

  fclose(cpuinfo);
  return detected;
}

int termux_hardware_get_config(struct termux_hardware_config *config) {
  if (!config) {
    return -1;
  }

  memset(config, 0, sizeof(*config));
  config->soc_type = termux_hardware_detect_soc();
  config->total_cores = (uint32_t)get_nprocs();

  switch (config->soc_type) {
    case TERMUX_SOC_SNAPDRAGON_888:
      config->big_cores = 3;
      config->little_cores = 4;
      config->golden.count = 1;
      config->golden.core_ids[0] = 7;  // Fastest core
      config->golden.priority_boost = 20;
      config->efficiency.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->efficiency.core_ids[i] = i;
      }
      config->max_freq_mhz = 2840;
      config->min_freq_mhz = 1800;
      config->has_neon = 1;
      break;

    case TERMUX_SOC_SNAPDRAGON_8GEN1:
      config->big_cores = 3;
      config->little_cores = 4;
      config->golden.count = 1;
      config->golden.core_ids[0] = 7;  // Fastest core
      config->golden.priority_boost = 25;
      config->efficiency.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->efficiency.core_ids[i] = i;
      }
      config->max_freq_mhz = 3200;
      config->min_freq_mhz = 1900;
      config->has_neon = 1;
      break;

    case TERMUX_SOC_APPLE_M1:
      config->big_cores = 4;
      config->little_cores = 4;
      config->golden.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->golden.core_ids[i] = i;  // P-cores (0-3)
      }
      config->golden.priority_boost = 30;
      config->efficiency.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->efficiency.core_ids[i] = 4 + i;  // E-cores (4-7)
      }
      config->max_freq_mhz = 3200;
      config->min_freq_mhz = 600;
      config->has_neon = 1;
      break;

    case TERMUX_SOC_APPLE_M2:
      config->big_cores = 4;
      config->little_cores = 4;
      config->golden.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->golden.core_ids[i] = i;  // P-cores (0-3)
      }
      config->golden.priority_boost = 35;
      config->efficiency.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->efficiency.core_ids[i] = 4 + i;  // E-cores (4-7)
      }
      config->max_freq_mhz = 3500;
      config->min_freq_mhz = 600;
      config->has_neon = 1;
      break;

    case TERMUX_SOC_EXYNOS_9820:
      config->big_cores = 2;
      config->little_cores = 6;
      config->golden.count = 2;
      for (uint32_t i = 0; i < 2; i++) {
        config->golden.core_ids[i] = i;  // M1, M2
      }
      config->golden.priority_boost = 15;
      config->efficiency.count = 6;
      for (uint32_t i = 0; i < 6; i++) {
        config->efficiency.core_ids[i] = 2 + i;  // LITTLE cores
      }
      config->max_freq_mhz = 2730;
      config->min_freq_mhz = 1500;
      config->has_neon = 1;
      break;

    case TERMUX_SOC_CORTEX_A73:
    case TERMUX_SOC_CORTEX_A53:
    default:
      config->big_cores = config->total_cores;
      config->little_cores = 0;
      config->golden.count = 1;
      config->golden.core_ids[0] = 0;
      config->golden.priority_boost = 10;
      config->max_freq_mhz = 2400;
      config->min_freq_mhz = 1200;
      config->has_neon = 1;
      break;
  }

  return 0;
}

int termux_hardware_enable_golden_cores(const struct termux_hardware_config *config) {
  if (!config || config->golden.count == 0) {
    return -1;
  }

  struct termux_cpu_mask mask;
  mask.cpu_count = config->golden.count;
  for (uint32_t i = 0; i < config->golden.count; i++) {
    mask.cpu_ids[i] = config->golden.core_ids[i];
  }

  return termux_cpu_affinity_mask_set(&mask);
}

int termux_hardware_enable_efficiency_cores(const struct termux_hardware_config *config) {
  if (!config || config->efficiency.count == 0) {
    return -1;
  }

  struct termux_cpu_mask mask;
  mask.cpu_count = config->efficiency.count;
  for (uint32_t i = 0; i < config->efficiency.count; i++) {
    mask.cpu_ids[i] = config->efficiency.core_ids[i];
  }

  return termux_cpu_affinity_mask_set(&mask);
}

uint32_t termux_hardware_get_frequency_boost(enum termux_soc_type soc) {
  switch (soc) {
    case TERMUX_SOC_SNAPDRAGON_8GEN1:
      return 25;
    case TERMUX_SOC_SNAPDRAGON_888:
      return 20;
    case TERMUX_SOC_APPLE_M2:
      return 35;
    case TERMUX_SOC_APPLE_M1:
      return 30;
    case TERMUX_SOC_EXYNOS_9820:
      return 15;
    default:
      return 10;
  }
}

const char *termux_hardware_soc_name(enum termux_soc_type soc) {
  switch (soc) {
    case TERMUX_SOC_CORTEX_A53:
      return "ARM Cortex-A53";
    case TERMUX_SOC_CORTEX_A73:
      return "ARM Cortex-A73";
    case TERMUX_SOC_SNAPDRAGON_888:
      return "Qualcomm Snapdragon 888";
    case TERMUX_SOC_SNAPDRAGON_8GEN1:
      return "Qualcomm Snapdragon 8 Gen 1";
    case TERMUX_SOC_APPLE_M1:
      return "Apple M1";
    case TERMUX_SOC_APPLE_M2:
      return "Apple M2";
    case TERMUX_SOC_EXYNOS_9820:
      return "Samsung Exynos 9820";
    case TERMUX_SOC_EXYNOS_2200:
      return "Samsung Exynos 2200";
    default:
      return "Unknown SoC";
  }
}
