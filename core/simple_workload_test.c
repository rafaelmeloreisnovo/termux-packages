#include "workload_generator.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  printf("Simple workload test\n\n");

  termux_workload_t workload = {0};
  printf("Allocating workload for 5 packages...\n");
  if (termux_workload_alloc(&workload, 5) != 0) {
    fprintf(stderr, "Failed to allocate\n");
    return 1;
  }

  printf("Populating packages...\n");
  if (termux_workload_populate_realistic(&workload) != 0) {
    fprintf(stderr, "Failed to populate\n");
    return 1;
  }

  printf("Packages created:\n");
  for (uint32_t i = 0; i < workload.pkg_count; i++) {
    termux_package_info_t *pkg = &workload.packages[i];
    printf("  [%u] %s: deps=%u -> ", i, pkg->name, pkg->dep_count);
    for (uint16_t d = 0; d < pkg->dep_count; d++) {
      printf("%u ", pkg->deps[d]);
      if (pkg->deps[d] >= workload.pkg_count) {
        printf("(INVALID!)");
      }
    }
    printf("\n");
  }

  printf("\nNow building graph...\n");
  if (termux_workload_build_graph(&workload) != 0) {
    fprintf(stderr, "Failed to build graph\n");
    termux_workload_free(&workload);
    return 1;
  }

  printf("Graph built successfully!\n");
  termux_workload_free(&workload);
  return 0;
}
