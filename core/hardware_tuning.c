#include "hardware_tuning.h"
#include "cpu_affinity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum termux_soc_type termux_hardware_detect_soc(void) {
  char buf[256] = {};
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (!f) return TERMUX_SOC_UNKNOWN;

  enum termux_soc_type detected = TERMUX_SOC_UNKNOWN;
  while (fgets(buf, sizeof(buf), f)) {
    if (strstr(buf, "Hardware") || strstr(buf, "processor")) {
      if (strstr(buf, "SM8350")) {
        detected = TERMUX_SOC_SNAPDRAGON_888;
        break;
      } else if (strstr(buf, "SM8450")) {
        detected = TERMUX_SOC_SNAPDRAGON_8GEN1;
        break;
      } else if (strstr(buf, "Apple")) {
        detected = TERMUX_SOC_APPLE_M1;
        break;
      } else if (strstr(buf, "Exynos")) {
        detected = TERMUX_SOC_EXYNOS_2200;
        break;
      }
    }
  }
  fclose(f);

  if (detected == TERMUX_SOC_UNKNOWN) {
    detected = TERMUX_SOC_CORTEX_A73;
  }

  return detected;
}

int termux_hardware_get_config(struct termux_hardware_config *config) {
  if (!config) return -1;

  memset(config, 0, sizeof(*config));
  config->soc_type = termux_hardware_detect_soc();
  config->max_freq_mhz = 2800;
  config->min_freq_mhz = 1000;
  config->has_neon = 1;

  uint32_t nprocs = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
  if (nprocs == 0 || nprocs > 8) nprocs = 4;
  config->total_cores = nprocs;

  switch (config->soc_type) {
    case TERMUX_SOC_SNAPDRAGON_888:
      config->big_cores = 1;
      config->little_cores = 3;
      config->golden.count = 1;
      config->golden.core_ids[0] = 0;
      config->golden.priority_boost = 20;
      config->efficiency.count = 3;
      config->efficiency.core_ids[0] = 1;
      config->efficiency.core_ids[1] = 2;
      config->efficiency.core_ids[2] = 3;
      config->max_freq_mhz = 3188;
      break;

    case TERMUX_SOC_SNAPDRAGON_8GEN1:
      config->big_cores = 2;
      config->little_cores = 5;
      config->golden.count = 1;
      config->golden.core_ids[0] = 0;
      config->golden.priority_boost = 25;
      config->efficiency.count = 6;
      for (uint32_t i = 0; i < 6; i++) {
        config->efficiency.core_ids[i] = (i + 1) % 7;
      }
      config->max_freq_mhz = 3200;
      break;

    case TERMUX_SOC_APPLE_M1:
      config->big_cores = 4;
      config->little_cores = 4;
      config->golden.count = 2;
      config->golden.core_ids[0] = 0;
      config->golden.core_ids[1] = 1;
      config->golden.priority_boost = 15;
      config->efficiency.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->efficiency.core_ids[i] = 4 + i;
      }
      config->max_freq_mhz = 3200;
      config->has_sve = 0;
      break;

    case TERMUX_SOC_APPLE_M2:
      config->big_cores = 4;
      config->little_cores = 4;
      config->golden.count = 2;
      config->golden.core_ids[0] = 0;
      config->golden.core_ids[1] = 1;
      config->golden.priority_boost = 20;
      config->efficiency.count = 4;
      for (uint32_t i = 0; i < 4; i++) {
        config->efficiency.core_ids[i] = 4 + i;
      }
      config->max_freq_mhz = 3500;
      config->has_sve = 0;
      break;

    case TERMUX_SOC_EXYNOS_2200:
      config->big_cores = 3;
      config->little_cores = 5;
      config->golden.count = 1;
      config->golden.core_ids[0] = 0;
      config->golden.priority_boost = 18;
      config->efficiency.count = 5;
      for (uint32_t i = 0; i < 5; i++) {
        config->efficiency.core_ids[i] = (i + 1) % 7;
      }
      config->max_freq_mhz = 2900;
      config->has_sve = 1;
      break;

    default:
      config->big_cores = config->total_cores / 2;
      config->little_cores = config->total_cores - config->big_cores;
      config->golden.count = 1;
      config->golden.core_ids[0] = 0;
      config->golden.priority_boost = 10;
      for (uint32_t i = 0; i < config->little_cores; i++) {
        config->efficiency.core_ids[i] = i + 1;
        if (i >= TERMUX_MAX_EFFICIENCY_CORES - 1) break;
      }
      config->efficiency.count = (config->little_cores > TERMUX_MAX_EFFICIENCY_CORES) ?
                                  TERMUX_MAX_EFFICIENCY_CORES : config->little_cores;
      break;
  }

  return 0;
}

int termux_hardware_enable_golden_cores(const struct termux_hardware_config *config) {
  if (!config) return -1;

  for (uint32_t i = 0; i < config->golden.count; i++) {
    uint32_t core_id = config->golden.core_ids[i];
    if (core_id >= config->total_cores) continue;

    int ret = termux_cpu_affinity_set(core_id);
    if (ret != 0) return -2;
  }

  return 0;
}

int termux_hardware_enable_efficiency_cores(const struct termux_hardware_config *config) {
  if (!config) return -1;

  if (config->efficiency.count == 0) return -2;

  for (uint32_t i = 0; i < config->efficiency.count; i++) {
    uint32_t core_id = config->efficiency.core_ids[i];
    if (core_id < config->total_cores) {
      int ret = termux_cpu_affinity_set(core_id);
      if (ret != 0) return -2;
    }
  }

  return 0;
}

uint32_t termux_hardware_get_frequency_boost(enum termux_soc_type soc) {
  switch (soc) {
    case TERMUX_SOC_SNAPDRAGON_888: return 20;
    case TERMUX_SOC_SNAPDRAGON_8GEN1: return 25;
    case TERMUX_SOC_APPLE_M1: return 15;
    case TERMUX_SOC_APPLE_M2: return 20;
    case TERMUX_SOC_EXYNOS_2200: return 18;
    default: return 10;
  }
}

const char *termux_hardware_soc_name(enum termux_soc_type soc) {
  switch (soc) {
    case TERMUX_SOC_SNAPDRAGON_888: return "Snapdragon 888";
    case TERMUX_SOC_SNAPDRAGON_8GEN1: return "Snapdragon 8 Gen 1";
    case TERMUX_SOC_APPLE_M1: return "Apple M1";
    case TERMUX_SOC_APPLE_M2: return "Apple M2";
    case TERMUX_SOC_EXYNOS_2200: return "Exynos 2200";
    default: return "Generic ARM64";
  }
}

int termux_platform_profile_init(hardware_context_t *ctx, enum termux_soc_type soc) {
  if (!ctx) return -1;

  memset(ctx, 0, sizeof(*ctx));
  ctx->profile.soc_type = soc;

  struct termux_hardware_config config = {};
  int ret = termux_hardware_get_config(&config);
  if (ret != 0) return ret;

  ctx->profile.core_count = config.total_cores;
  ctx->profile.max_freq_mhz = config.max_freq_mhz;
  ctx->profile.min_freq_mhz = config.min_freq_mhz;
  ctx->profile.has_neon = config.has_neon;
  ctx->profile.has_sve = config.has_sve;
  ctx->profile.has_dotprod = (soc == TERMUX_SOC_SNAPDRAGON_888 || soc == TERMUX_SOC_EXYNOS_2200);

  if (config.golden.count > 0) {
    ctx->profile.golden_core_id = config.golden.core_ids[0];
  }

  ctx->profile.perf_core_mask = 0;
  for (uint32_t i = 0; i < config.big_cores && i < 8; i++) {
    ctx->profile.perf_core_mask |= (1U << i);
  }

  ctx->profile.effi_core_mask = 0;
  for (uint32_t i = config.big_cores; i < config.total_cores && i < 8; i++) {
    ctx->profile.effi_core_mask |= (1U << i);
  }

  ctx->profile.l3_cache_kb = 8192;
  ctx->profile.performance_scaling = 1.0 + (config.big_cores * 0.15);
  ctx->profile.efficiency_scaling = 1.0 + (config.little_cores * 0.10);

  for (uint32_t i = 0; i < config.total_cores; i++) {
    ctx->core_frequencies[i] = config.max_freq_mhz;
  }

  return 0;
}

int termux_golden_core_prioritize(hardware_context_t *ctx, uint32_t pkg_idx) {
  if (!ctx) return -1;

  uint32_t golden_id = ctx->profile.golden_core_id;
  if (golden_id >= ctx->profile.core_count) return -2;

  ctx->core_frequencies[golden_id] = ctx->profile.max_freq_mhz;
  ctx->core_utilization[golden_id] += 1;

  for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
    if (i != golden_id) {
      if (ctx->core_frequencies[i] > ctx->profile.min_freq_mhz + 400) {
        ctx->core_frequencies[i] -= 200;
      }
    }
  }

  if (ctx->scheduled_count < 256) {
    ctx->scheduled_packages[ctx->scheduled_count++] = pkg_idx;
  }

  return 0;
}

int termux_efficiency_core_bypass(hardware_context_t *ctx, uint32_t _pkg_idx) {
  (void)_pkg_idx;
  if (!ctx) return -1;

  uint32_t efficiency_mask = ctx->profile.effi_core_mask;
  if (efficiency_mask == 0) return -2;

  uint32_t best_core = __builtin_ctz(efficiency_mask);
  if (best_core >= 8) return -3;

  for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
    if (ctx->core_utilization[i] < ctx->core_utilization[best_core]) {
      if ((efficiency_mask & (1U << i)) != 0) {
        best_core = i;
      }
    }
  }

  ctx->core_utilization[best_core]++;
  ctx->core_frequencies[best_core] = ctx->profile.min_freq_mhz + 200;

  return 0;
}

int termux_big_little_load_balance(hardware_context_t *ctx, uint32_t _layer_idx) {
  (void)_layer_idx;
  if (!ctx) return -1;

  uint64_t total_util_perf = 0, total_util_effi = 0;
  uint32_t perf_count = 0, effi_count = 0;

  for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
    if (ctx->profile.perf_core_mask & (1U << i)) {
      total_util_perf += ctx->core_utilization[i];
      perf_count++;
    } else if (ctx->profile.effi_core_mask & (1U << i)) {
      total_util_effi += ctx->core_utilization[i];
      effi_count++;
    }
  }

  if (perf_count == 0 || effi_count == 0) return 0;

  double avg_util_perf = (double)total_util_perf / perf_count;
  double avg_util_effi = (double)total_util_effi / effi_count;

  if (avg_util_perf > avg_util_effi * 1.5) {
    for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
      if (ctx->profile.perf_core_mask & (1U << i)) {
        if (ctx->core_frequencies[i] > ctx->profile.min_freq_mhz + 600) {
          ctx->core_frequencies[i] -= 300;
        }
      }
    }
  } else if (avg_util_effi > avg_util_perf * 1.3) {
    for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
      if (ctx->profile.effi_core_mask & (1U << i)) {
        if (ctx->core_frequencies[i] < ctx->profile.max_freq_mhz - 500) {
          ctx->core_frequencies[i] += 200;
        }
      }
    }
  }

  return 0;
}

int termux_neon_mapping_optimize(hardware_context_t *ctx) {
  if (!ctx || !ctx->profile.has_neon) return -1;

  for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
    ctx->coherence_scores[i] = 0.90;
  }

  return 0;
}

double termux_coherence_score_platform(const hardware_context_t *ctx, uint32_t core_id) {
  if (!ctx || core_id >= ctx->profile.core_count) return 0.0;

  double base_score = ctx->coherence_scores[core_id];
  double freq_factor = (double)ctx->core_frequencies[core_id] / ctx->profile.max_freq_mhz;
  double util_factor = 1.0 - ((double)ctx->core_utilization[core_id] / 256.0);

  if (util_factor < 0.0) util_factor = 0.0;

  return base_score * freq_factor * util_factor;
}

void termux_platform_print_profile(const hardware_context_t *ctx) {
  if (!ctx) return;

  printf("\n");
  printf("================================================================================\n");
  printf("                      HARDWARE PLATFORM PROFILE\n");
  printf("================================================================================\n");
  printf("Platform: %s\n", termux_hardware_soc_name(ctx->profile.soc_type));
  printf("Total Cores: %u\n", ctx->profile.core_count);
  printf("Max Frequency: %u MHz\n", ctx->profile.max_freq_mhz);
  printf("L3 Cache: %u KB\n", ctx->profile.l3_cache_kb);

  printf("\nFeature Flags:\n");
  printf("  NEON: %s\n", ctx->profile.has_neon ? "Yes" : "No");
  printf("  SVE: %s\n", ctx->profile.has_sve ? "Yes" : "No");
  printf("  DOT-PROD: %s\n", ctx->profile.has_dotprod ? "Yes" : "No");

  printf("\nPer-Core Status:\n");
  for (uint32_t i = 0; i < ctx->profile.core_count; i++) {
    const char *type = (ctx->profile.perf_core_mask & (1U << i)) ? "PERF" : "EFFI";
    printf("  Core %u [%s]: freq=%u MHz, util=%lu, φ=%.2f\n",
           i, type, ctx->core_frequencies[i],
           ctx->core_utilization[i],
           ctx->coherence_scores[i]);
  }

  printf("\nLoad Balance:\n");
  printf("  Performance scaling: %.2fx\n", ctx->profile.performance_scaling);
  printf("  Efficiency scaling: %.2fx\n", ctx->profile.efficiency_scaling);

  printf("================================================================================\n\n");
}
