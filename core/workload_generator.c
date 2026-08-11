#include "workload_generator.h"
#include "build_orchestrator.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

static const char *base_packages[] = {
  "ncurses", "openssl", "readline", "zlib", "bzip2",
  "xz-utils", "libffi", "libunistring", "libiconv", "libtasn1",
  "libgpg-error", "libgcrypt", "pcre", "libxml2", "libxslt"
};

static const char *pkg_name_templates[] = {
  "gnu-%s", "lib%s-dev", "%s-utils", "%s-tools", "%s-doc",
  "%s-bin", "%s-lib", "%s-core", "%s-extra", "%s-static"
};

static uint32_t fnv_hash(const char *str, uint32_t seed) {
  uint32_t hash = 2166136261U ^ seed;
  for (size_t i = 0; str[i]; i++) {
    hash ^= (uint8_t)str[i];
    hash *= 16777619U;
  }
  return hash;
}

int termux_workload_alloc(termux_workload_t *wl, uint32_t pkg_count) {
  if (!wl || pkg_count == 0 || pkg_count > TERMUX_REAL_PKG_COUNT) return -1;

  wl->pkg_count = pkg_count;
  wl->packages = (termux_package_info_t *)calloc(pkg_count, sizeof(termux_package_info_t));
  if (!wl->packages) return -2;

  wl->resolver = (struct termux_dep_resolver *)malloc(sizeof(struct termux_dep_resolver));
  if (!wl->resolver) {
    free(wl->packages);
    return -2;
  }

  memset(wl->resolver, 0, sizeof(*wl->resolver));
  return 0;
}

void termux_workload_free(termux_workload_t *wl) {
  if (!wl) return;
  if (wl->packages) free(wl->packages);
  if (wl->resolver) {
    termux_dep_resolver_destroy(wl->resolver);
    free(wl->resolver);
  }
}

int termux_workload_populate_realistic(termux_workload_t *wl) {
  if (!wl || !wl->packages) return -1;

  uint32_t base_count = sizeof(base_packages) / sizeof(base_packages[0]);

  for (uint32_t i = 0; i < wl->pkg_count; i++) {
    termux_package_info_t *pkg = &wl->packages[i];

    uint32_t hash = fnv_hash("pkg", i);
    uint32_t base_idx = hash % base_count;
    uint32_t template_idx = (hash >> 8) % (sizeof(pkg_name_templates) / sizeof(pkg_name_templates[0]));

    snprintf(pkg->name, sizeof(pkg->name), pkg_name_templates[template_idx],
             base_packages[base_idx]);

    pkg->build_time_ms = 100 + (hash % 5000);
    pkg->install_size_kb = 50 + (hash % 10000);

    uint32_t dep_hash = fnv_hash(pkg->name, i + 1);
    pkg->dep_count = 1 + ((dep_hash / 7) % 15);
    if (pkg->dep_count > 16) pkg->dep_count = 16;

    for (uint16_t d = 0; d < pkg->dep_count; d++) {
      uint32_t dep_seed = fnv_hash(pkg->name, i + d + 42);
      uint32_t dep_idx;
      if (i > 0) {
        dep_idx = (dep_seed / (d + 1)) % i;
      } else {
        dep_idx = 0;
      }
      if (dep_idx >= wl->pkg_count) dep_idx = wl->pkg_count - 1;

      pkg->deps[d] = (uint16_t)dep_idx;
    }

    wl->total_edges += pkg->dep_count;
  }

  return 0;
}

int termux_workload_build_graph(termux_workload_t *wl) {
  if (!wl || !wl->packages || !wl->resolver) return -1;

  int ret = termux_dep_resolver_init(wl->resolver, wl->pkg_count);
  if (ret != 0) return ret;

  for (uint32_t i = 0; i < wl->pkg_count; i++) {
    termux_package_info_t *pkg = &wl->packages[i];
    for (uint16_t d = 0; d < pkg->dep_count; d++) {
      ret = termux_dep_graph_add_edge(&wl->resolver->graph, pkg->deps[d], i);
      if (ret != 0) return ret;
    }
  }

  ret = termux_dep_graph_finalize_csr(&wl->resolver->graph);
  if (ret != 0) return ret;

  ret = termux_dep_resolver_build_layers(wl->resolver);
  if (ret != 0) return ret;

  return 0;
}

uint32_t termux_workload_validate_dag(const termux_workload_t *wl) {
  if (!wl || !wl->resolver) return 0;

  uint32_t errors = 0;

  for (uint32_t i = 0; i < wl->pkg_count; i++) {
    termux_package_info_t *pkg = &wl->packages[i];

    for (uint16_t d = 0; d < pkg->dep_count; d++) {
      if (pkg->deps[d] >= wl->pkg_count) {
        errors++;
        printf("✗ Package %u: invalid dep[%u] = %u\n", i, d, pkg->deps[d]);
      }
    }

    uint8_t depth = wl->resolver->graph.depths[i];
    if (depth >= TERMUX_DAG_LAYERS) {
      errors++;
      printf("✗ Package %u: depth %u exceeds max %u\n", i, depth, TERMUX_DAG_LAYERS - 1);
    }
  }

  return errors;
}

void termux_workload_print_stats(const termux_workload_t *wl) {
  if (!wl || !wl->resolver) return;

  printf("\n=== Workload Statistics ===\n");
  printf("Total packages: %u\n", wl->pkg_count);
  printf("Total edges: %" PRIu64 "\n", wl->total_edges);
  printf("Avg deps/pkg: %.2f\n", (double)wl->total_edges / (double)wl->pkg_count);

  uint64_t total_build_time = 0, total_install_size = 0;
  for (uint32_t i = 0; i < wl->pkg_count; i++) {
    total_build_time += wl->packages[i].build_time_ms;
    total_install_size += wl->packages[i].install_size_kb;
  }

  printf("Total build time (sequential): %.2f hours\n",
         (double)total_build_time / 3600000.0);
  printf("Total install size: %.2f GB\n",
         (double)total_install_size / 1024.0 / 1024.0);

  if (wl->resolver) {
    printf("Graph CRC32c: 0x%08x\n", wl->resolver->graph.crc32c_checksum);
    printf("DAG layers used: %u / %u\n", TERMUX_DAG_LAYERS, TERMUX_DAG_LAYERS);
  }

  printf("\n");
}
