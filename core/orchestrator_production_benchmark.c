#include "build_orchestrator_production.c"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  uint32_t num_threads = 4;

  if (argc > 1) {
    num_threads = (uint32_t)atoi(argv[1]);
    if (num_threads == 0 || num_threads > 128) {
      num_threads = 4;
    }
  }

  printf("Production Build Orchestrator Benchmark\n");
  printf("========================================\n\n");

  production_build_context_t ctx;
  if (termux_orchestrator_production_init(&ctx, num_threads) != 0) {
    fprintf(stderr, "Error: Failed to initialize production context\n");
    return 1;
  }

  if (termux_orchestrator_production_run(&ctx) != 0) {
    fprintf(stderr, "Error: Production build failed\n");
    return 1;
  }

  printf("\nBenchmark Summary:\n");
  printf("  Thread count: %u\n", num_threads);
  printf("  Final speedup: %.2fx\n", ctx.speedup);
  printf("  Final coherence φ: %.4f\n", ctx.mean_phi);

  return 0;
}
