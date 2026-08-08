#ifndef TERMUX_WORKLOAD_GENERATOR_H
#define TERMUX_WORKLOAD_GENERATOR_H

#include "dep_resolver.h"
#include <stdint.h>

#define TERMUX_REAL_PKG_COUNT 2057
#define TERMUX_AVG_DEPS_PER_PKG 7

typedef struct {
  char name[64];
  uint16_t dep_count;
  uint16_t deps[16];
  uint32_t build_time_ms;
  uint32_t install_size_kb;
} termux_package_info_t;

typedef struct {
  termux_package_info_t *packages;
  uint32_t pkg_count;
  struct termux_dep_resolver *resolver;
  uint64_t total_edges;
} termux_workload_t;

int termux_workload_alloc(termux_workload_t *wl, uint32_t pkg_count);

void termux_workload_free(termux_workload_t *wl);

int termux_workload_populate_realistic(termux_workload_t *wl);

int termux_workload_build_graph(termux_workload_t *wl);

uint32_t termux_workload_validate_dag(const termux_workload_t *wl);

void termux_workload_print_stats(const termux_workload_t *wl);

#endif
