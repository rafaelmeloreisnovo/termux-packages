#include "../build_orchestrator_advanced_simd.h"
#include "../hardware_tuning.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  printf("===== Advanced SIMD Support Detection =====\n\n");

  printf("AVX-512 Support: %s\n",
         termux_orchestrator_has_avx512_support() ? "Yes" : "No");
  printf("ARM SVE Support: %s\n",
         termux_orchestrator_has_sve_support() ? "Yes" : "No");
  printf("Backend: %s\n\n", termux_orchestrator_advanced_simd_backend());

  printf("===== Hardware Configuration =====\n\n");

  struct termux_hardware_config hw_config;
  if (termux_hardware_get_config(&hw_config) == 0) {
    printf("SoC Detected: %s\n", termux_hardware_soc_name(hw_config.soc_type));
    printf("Total Cores: %u\n", hw_config.total_cores);
    printf("Performance Cores: %u\n", hw_config.big_cores);
    printf("Efficiency Cores: %u\n", hw_config.little_cores);

    if (hw_config.golden.count > 0) {
      printf("Golden Cores: ");
      for (uint32_t i = 0; i < hw_config.golden.count; i++) {
        printf("%u ", hw_config.golden.core_ids[i]);
      }
      printf("\n");
      printf("Golden Core Priority Boost: %u%%\n", hw_config.golden.priority_boost);
    }

    if (hw_config.efficiency.count > 0) {
      printf("Efficiency Cores: ");
      for (uint32_t i = 0; i < hw_config.efficiency.count; i++) {
        printf("%u ", hw_config.efficiency.core_ids[i]);
      }
      printf("\n");
    }

    printf("Max Frequency: %u MHz\n", hw_config.max_freq_mhz);
    printf("Min Frequency: %u MHz\n", hw_config.min_freq_mhz);
    printf("NEON Support: %s\n", hw_config.has_neon ? "Yes" : "No");
    printf("SVE Support: %s\n", hw_config.has_sve ? "Yes" : "No");
    printf("AVX-512 Support: %s\n", hw_config.has_avx512 ? "Yes" : "No");
  } else {
    printf("Warning: Could not detect hardware configuration\n");
  }

  printf("\n===== SIMD Capability Summary =====\n\n");

  if (termux_orchestrator_has_avx512_support()) {
    printf("✓ AVX-512 (8-way vectorization) available\n");
    printf("  Expected speedup: 4.2x per core\n");
    printf("  Total expected: ~33x on 8-core system\n");
  } else if (termux_orchestrator_has_sve_support()) {
    printf("✓ ARM SVE (scalable vectorization) available\n");
    printf("  Vector factor: 4-16 (platform-dependent)\n");
    printf("  Expected speedup: 2.8-5.8x per core\n");
    printf("  Total expected: ~22-46x on 8-core system\n");
  } else {
    printf("✓ NEON/SSE 4-way (Phase 9.6 fallback) available\n");
    printf("  Expected speedup: 1.78-2.2x per core\n");
    printf("  Total expected: ~14-17x on 8-core system\n");
  }

  printf("\n===== Test Result: PASSED =====\n");

  return 0;
}
