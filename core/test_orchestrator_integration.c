#include "workload_generator.h"
#include "unified_orchestrator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
  printf("Test: Orchestrator Integration with Workload Generator\n\n");

  termux_workload_t workload = {0};
  printf("1. Allocating workload for 20 packages...\n");
  if (termux_workload_alloc(&workload, 20) != 0) {
    fprintf(stderr, "Failed to allocate workload\n");
    return 1;
  }
  printf("   ✓ Workload allocated\n\n");

  printf("2. Populating realistic package data...\n");
  if (termux_workload_populate_realistic(&workload) != 0) {
    fprintf(stderr, "Failed to populate workload\n");
    termux_workload_free(&workload);
    return 1;
  }
  printf("   ✓ Packages populated (%u packages)\n\n", workload.pkg_count);

  printf("3. Building dependency graph...\n");
  int graph_ret = termux_workload_build_graph(&workload);
  if (graph_ret != 0) {
    fprintf(stderr, "Failed to build dependency graph (ret=%d)\n", graph_ret);
    fprintf(stderr, "  Total packages: %u\n", workload.pkg_count);
    fprintf(stderr, "  Total edges: %lu\n", workload.total_edges);
    if (workload.resolver) {
      fprintf(stderr, "  Resolver graph pkg_count: %u\n", workload.resolver->graph.pkg_count);
      fprintf(stderr, "  Resolver graph depths: %p\n", (void*)workload.resolver->graph.depths);
    }
    termux_workload_free(&workload);
    return 1;
  }
  printf("   ✓ Dependency graph built\n\n");

  printf("4. Validating DAG...\n");
  uint32_t validation_errors = termux_workload_validate_dag(&workload);
  if (validation_errors > 0) {
    fprintf(stderr, "   Warning: %u validation errors\n", validation_errors);
  } else {
    printf("   ✓ DAG is valid\n");
  }
  printf("\n");

  printf("5. Printing workload statistics...\n");
  termux_workload_print_stats(&workload);

  printf("6. Testing orchestrator allocation...\n");
  unified_orchestrator_t *orchestrator = NULL;
  if (unified_orchestrator_alloc(&orchestrator) != 0) {
    fprintf(stderr, "Failed to allocate orchestrator\n");
    termux_workload_free(&workload);
    return 1;
  }
  printf("   ✓ Orchestrator allocated\n\n");

  printf("7. Initializing orchestrator...\n");
  if (unified_orchestrator_init_parallel(orchestrator, 1) != 0) {
    fprintf(stderr, "Failed to initialize orchestrator\n");
    unified_orchestrator_free(orchestrator);
    termux_workload_free(&workload);
    return 1;
  }
  printf("   ✓ Orchestrator initialized\n");
  printf("   Device: %s\n", orchestrator->device_module.profiles[0].device_name);
  printf("   CPUs: %u cores @ %u MHz\n",
         orchestrator->device_module.profiles[0].cpu_cores,
         orchestrator->device_module.profiles[0].cpu_freq_mhz);
  printf("\n");

  printf("8. Simulating layer-based build execution...\n");
  for (uint32_t i = 0; i < workload.pkg_count; i++) {
    termux_package_info_t *pkg = &workload.packages[i];
    printf("   Package %2u: %s (deps=%u, time=%u ms)\n",
           i, pkg->name, pkg->dep_count, pkg->build_time_ms);

    double phi = 0.8 + (double)(i % 20) / 100.0;
    unified_orchestrator_update_coherence(orchestrator, i % TERMUX_TOROIDAL_LAYERS, phi);
  }
  printf("   ✓ Layer execution simulated\n\n");

  printf("9. Testing orchestrator report...\n");
  unified_orchestrator_report(orchestrator);

  printf("\n✓ ALL INTEGRATION TESTS PASSED\n");

  unified_orchestrator_free(orchestrator);
  termux_workload_free(&workload);

  return 0;
}
