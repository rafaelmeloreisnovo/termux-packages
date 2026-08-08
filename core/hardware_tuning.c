#include "hardware_tuning.h"
#include "cpu_affinity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * OBSERVED_LIMITED hardware adapter.
 *
 * This module no longer fabricates a Generic ARM64/Cortex-A73 profile when the
 * running host is unknown. CPU count, feature flags and cpufreq data are used
 * only when observable. Unknown values remain zero/UNKNOWN and callers must not
 * promote them to device evidence.
 */

static int cpuinfo_has_token(const char *wanted) {
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (!f) return 0;

  char line[1024];
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    char *colon = strchr(line, ':');
    if (!colon) continue;
    char *key = line;
    *colon = '\0';
    if (strstr(key, "flags") == NULL && strstr(key, "Flags") == NULL &&
        strstr(key, "features") == NULL && strstr(key, "Features") == NULL)
      continue;

    char *save = NULL;
    for (char *tok = strtok_r(colon + 1, " \t\r\n", &save);
         tok != NULL;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
      if (strcmp(tok, wanted) == 0) {
        found = 1;
        break;
      }
    }
    if (found) break;
  }
  fclose(f);
  return found;
}

static uint32_t read_cpu_freq_mhz(uint32_t cpu, const char *field) {
  char path[256];
  snprintf(path, sizeof(path),
           "/sys/devices/system/cpu/cpu%u/cpufreq/%s", cpu, field);
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  unsigned long khz = 0;
  int ok = fscanf(f, "%lu", &khz);
  fclose(f);
  if (ok != 1 || khz == 0) return 0;
  return (uint32_t)(khz / 1000UL);
}

enum termux_soc_type termux_hardware_detect_soc(void) {
  char buf[512] = {};
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (!f) return TERMUX_SOC_UNKNOWN;

  enum termux_soc_type detected = TERMUX_SOC_UNKNOWN;
  while (fgets(buf, sizeof(buf), f)) {
    if (strstr(buf, "SM8350")) {
      detected = TERMUX_SOC_SNAPDRAGON_888;
      break;
    }
    if (strstr(buf, "SM8450")) {
      detected = TERMUX_SOC_SNAPDRAGON_8GEN1;
      break;
    }
    if (strstr(buf, "Apple M1")) {
      detected = TERMUX_SOC_APPLE_M1;
      break;
    }
    if (strstr(buf, "Apple M2")) {
      detected = TERMUX_SOC_APPLE_M2;
      break;
    }
    if (strstr(buf, "Exynos 2200")) {
      detected = TERMUX_SOC_EXYNOS_2200;
      break;
    }
    if (strstr(buf, "Exynos 9820")) {
      detected = TERMUX_SOC_EXYNOS_9820;
      break;
    }
    if (strstr(buf, "Cortex-A53")) {
      detected = TERMUX_SOC_CORTEX_A53;
      break;
    }
    if (strstr(buf, "Cortex-A73")) {
      detected = TERMUX_SOC_CORTEX_A73;
      break;
    }
  }
  fclose(f);
  return detected;
}

int termux_hardware_get_config(struct termux_hardware_config *config) {
  if (!config) return -1;

  memset(config, 0, sizeof(*config));
  config->soc_type = termux_hardware_detect_soc();

  long nprocs_raw = sysconf(_SC_NPROCESSORS_ONLN);
  if (nprocs_raw <= 0) return -2;
  uint32_t nprocs = (uint32_t)nprocs_raw;
  if (nprocs > TERMUX_MAX_CORES) nprocs = TERMUX_MAX_CORES;
  config->total_cores = nprocs;

  config->has_neon = (uint8_t)(cpuinfo_has_token("neon") ||
                               cpuinfo_has_token("asimd"));
  config->has_sve = (uint8_t)cpuinfo_has_token("sve");
  config->has_avx512 = (uint8_t)cpuinfo_has_token("avx512f");

  uint32_t per_core_max[TERMUX_MAX_CORES] = {};
  uint32_t observed_max = 0;
  uint32_t observed_min = 0;
  uint32_t freq_observed = 0;

  for (uint32_t i = 0; i < nprocs; i++) {
    uint32_t max_mhz = read_cpu_freq_mhz(i, "cpuinfo_max_freq");
    uint32_t min_mhz = read_cpu_freq_mhz(i, "cpuinfo_min_freq");
    per_core_max[i] = max_mhz;
    if (max_mhz > 0) {
      freq_observed++;
      if (max_mhz > observed_max) observed_max = max_mhz;
    }
    if (min_mhz > 0 && (observed_min == 0 || min_mhz < observed_min))
      observed_min = min_mhz;
  }

  config->max_freq_mhz = observed_max;
  config->min_freq_mhz = observed_min;

  /* Infer a big/little split only from observed per-core maximum frequency.
   * No cpufreq evidence => no topology claim. */
  if (freq_observed > 0 && observed_max > 0) {
    const uint32_t threshold = (observed_max * 90U) / 100U;
    for (uint32_t i = 0; i < nprocs; i++) {
      if (per_core_max[i] == 0) continue;
      if (per_core_max[i] >= threshold) {
        config->big_cores++;
        if (config->golden.count == 0) {
          config->golden.core_ids[0] = i;
          config->golden.count = 1;
        }
      } else {
        config->little_cores++;
        if (config->efficiency.count < TERMUX_MAX_EFFICIENCY_CORES) {
          config->efficiency.core_ids[config->efficiency.count++] = i;
        }
      }
    }
  }

  /* No performance boost is asserted from model names alone. */
  config->golden.priority_boost = 0;
  config->efficiency.energy_limit = 0;
  return 0;
}

int termux_hardware_enable_golden_cores(const struct termux_hardware_config *config) {
  if (!config) return -1;
  if (config->golden.count == 0) return -2;

  for (uint32_t i = 0; i < config->golden.count; i++) {
    uint32_t core_id = config->golden.core_ids[i];
    if (core_id >= config->total_cores) return -3;
    int ret = termux_cpu_affinity_set(core_id);
    if (ret != 0) return -4;
  }
  return 0;
}

int termux_hardware_enable_efficiency_cores(const struct termux_hardware_config *config) {
  if (!config) return -1;
  if (config->efficiency.count == 0) return -2;

  for (uint32_t i = 0; i < config->efficiency.count; i++) {
    uint32_t core_id = config->efficiency.core_ids[i];
    if (core_id >= config->total_cores) return -3;
    int ret = termux_cpu_affinity_set(core_id);
    if (ret != 0) return -4;
  }
  return 0;
}

uint32_t termux_hardware_get_frequency_boost(enum termux_soc_type soc) {
  (void)soc;
  /* A boost percentage is a policy/measurement result, not an SoC identity
   * invariant. Return 0 until a benchmark/thermal receipt defines one. */
  return 0;
}

const char *termux_hardware_soc_name(enum termux_soc_type soc) {
  switch (soc) {
    case TERMUX_SOC_CORTEX_A53: return "Cortex-A53 (observed token)";
    case TERMUX_SOC_CORTEX_A73: return "Cortex-A73 (observed token)";
    case TERMUX_SOC_SNAPDRAGON_888: return "Snapdragon 888 / SM8350 (observed token)";
    case TERMUX_SOC_SNAPDRAGON_8GEN1: return "Snapdragon 8 Gen 1 / SM8450 (observed token)";
    case TERMUX_SOC_APPLE_M1: return "Apple M1 (observed token)";
    case TERMUX_SOC_APPLE_M2: return "Apple M2 (observed token)";
    case TERMUX_SOC_EXYNOS_9820: return "Exynos 9820 (observed token)";
    case TERMUX_SOC_EXYNOS_2200: return "Exynos 2200 (observed token)";
    case TERMUX_SOC_UNKNOWN:
    default: return "UNKNOWN / TOKEN_VAZIO_SOC_IDENTITY";
  }
}

int termux_platform_profile_init(hardware_context_t *ctx, enum termux_soc_type soc) {
  if (!ctx) return -1;
  (void)soc;

  memset(ctx, 0, sizeof(*ctx));

  struct termux_hardware_config config = {};
  int ret = termux_hardware_get_config(&config);
  if (ret != 0) return ret;

  ctx->profile.soc_type = config.soc_type;
  ctx->profile.core_count = config.total_cores;
  ctx->profile.max_freq_mhz = config.max_freq_mhz;
  ctx->profile.min_freq_mhz = config.min_freq_mhz;
  ctx->profile.has_neon = config.has_neon;
  ctx->profile.has_sve = config.has_sve;
  ctx->profile.has_dotprod = cpuinfo_has_token("asimddp") ||
                             cpuinfo_has_token("dotprod");

  ctx->profile.golden_core_id = 0;
  if (config.golden.count > 0)
    ctx->profile.golden_core_id = config.golden.core_ids[0];

  ctx->profile.effi_core_mask = 0;
  for (uint32_t i = 0; i < config.efficiency.count; i++) {
    uint32_t core_id = config.efficiency.core_ids[i];
    if (core_id < 8) ctx->profile.effi_core_mask |= (1U << core_id);
  }

  ctx->profile.perf_core_mask = 0;
  if (config.big_cores > 0) {
    for (uint32_t i = 0; i < config.total_cores && i < 8; i++) {
      if ((ctx->profile.effi_core_mask & (1U << i)) == 0)
        ctx->profile.perf_core_mask |= (1U << i);
    }
  }

  ctx->profile.l3_cache_kb = 0; /* not probed by this module */
  ctx->profile.performance_scaling = 1.0;
  ctx->profile.efficiency_scaling = 1.0;

  for (uint32_t i = 0; i < config.total_cores; i++)
    ctx->core_frequencies[i] = config.max_freq_mhz;

  return 0;
}

int termux_golden_core_prioritize(hardware_context_t *ctx, uint32_t pkg_idx) {
  if (!ctx) return -1;
  if (ctx->profile.perf_core_mask == 0 || ctx->profile.max_freq_mhz == 0)
    return -2;

  uint32_t golden_id = ctx->profile.golden_core_id;
  if (golden_id >= ctx->profile.core_count) return -3;

  /* This updates the scheduler model only; it does not write cpufreq sysfs. */
  ctx->core_frequencies[golden_id] = ctx->profile.max_freq_mhz;
  ctx->core_utilization[golden_id] += 1;

  if (ctx->scheduled_count < 256)
    ctx->scheduled_packages[ctx->scheduled_count++] = pkg_idx;
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
    if ((efficiency_mask & (1U << i)) != 0 &&
        ctx->core_utilization[i] < ctx->core_utilization[best_core])
      best_core = i;
  }

  ctx->core_utilization[best_core]++;
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

  /* No observed heterogeneous topology => nothing to balance. */
  if (perf_count == 0 || effi_count == 0) return 0;

  /* The model records imbalance only; it does not claim hardware DVFS writes. */
  double avg_util_perf = (double)total_util_perf / perf_count;
  double avg_util_effi = (double)total_util_effi / effi_count;
  (void)avg_util_perf;
  (void)avg_util_effi;
  return 0;
}

int termux_neon_mapping_optimize(hardware_context_t *ctx) {
  if (!ctx || !ctx->profile.has_neon) return -1;

  /* Mark model eligibility only; no NEON instruction execution is proven here. */
  for (uint32_t i = 0; i < ctx->profile.core_count; i++)
    ctx->coherence_scores[i] = 0.0;
  return 0;
}

double termux_coherence_score_platform(const hardware_context_t *ctx, uint32_t core_id) {
  if (!ctx || core_id >= ctx->profile.core_count) return 0.0;
  if (ctx->profile.max_freq_mhz == 0) return 0.0;

  double base_score = ctx->coherence_scores[core_id];
  double freq_factor = (double)ctx->core_frequencies[core_id] /
                       (double)ctx->profile.max_freq_mhz;
  double util_factor = 1.0 - ((double)ctx->core_utilization[core_id] / 256.0);
  if (util_factor < 0.0) util_factor = 0.0;
  return base_score * freq_factor * util_factor;
}

void termux_platform_print_profile(const hardware_context_t *ctx) {
  if (!ctx) return;

  printf("\n");
  printf("================================================================================\n");
  printf("             OBSERVED-LIMITED HARDWARE PROFILE (NOT DEVICE PROOF)\n");
  printf("================================================================================\n");
  printf("SoC identity: %s\n", termux_hardware_soc_name(ctx->profile.soc_type));
  printf("Online cores observed: %u\n", ctx->profile.core_count);
  if (ctx->profile.max_freq_mhz > 0)
    printf("Observed cpufreq max: %u MHz\n", ctx->profile.max_freq_mhz);
  else
    printf("Observed cpufreq max: TOKEN_VAZIO\n");
  printf("L3 Cache: TOKEN_VAZIO (not probed here)\n");

  printf("\nObserved feature tokens:\n");
  printf("  NEON/ASIMD: %s\n", ctx->profile.has_neon ? "observed" : "not observed");
  printf("  SVE: %s\n", ctx->profile.has_sve ? "observed" : "not observed");
  printf("  DOT-PROD: %s\n", ctx->profile.has_dotprod ? "observed" : "not observed");

  printf("\nclaim_allowed=false\n");
  printf("physical_device_verified=false\n");
  printf("================================================================================\n\n");
}
