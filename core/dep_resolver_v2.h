#ifndef TERMUX_DEP_RESOLVER_V2_H
#define TERMUX_DEP_RESOLVER_V2_H

#include <stdint.h>
#include <stddef.h>

/*
 * Phase 3: Dependency Resolver V2
 * Toroidal DAG Traversal with Sparse Matrix (CSR Format)
 *
 * Topology: 42 layers (7 phases × 6 architectures)
 * Distribution: ~49 packages per layer (2057 / 42 ≈ 49)
 *
 * Memory Layout (Compressed Sparse Row):
 *   - deps_row[2058]: Offsets (row pointers)
 *   - deps_col[~14000]: Column indices (dep package indices)
 *   - depths[2058]: Topological depth per package
 *
 * Total: ~56 KB memory footprint
 */

#define TERMUX_DEP_RESOLVER_V2_VERSION "3.0.0"
#define TERMUX_PACKAGE_COUNT 2057
#define TERMUX_LAYER_COUNT 42
#define TERMUX_MAX_DEPS 7        /* Average dependencies per package */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

typedef struct {
  uint16_t *deps_row;           /* [TERMUX_PACKAGE_COUNT + 1] row offsets */
  uint16_t *deps_col;           /* [~14000] column indices (sparse) */
  uint8_t  *depths;             /* [TERMUX_PACKAGE_COUNT] topological depth */
  uint32_t pkg_count;           /* 2057 */
  uint32_t dep_count;           /* ~14000 (total dependency edges) */
  uint8_t *layer_assignment;    /* [TERMUX_PACKAGE_COUNT] layer 0..41 */
  uint64_t completion_bitmap;   /* Progress tracking (64 packages) */
} dep_graph_v2_t;

typedef struct {
  uint32_t pkg_idx;             /* Package index */
  uint32_t layer_idx;           /* Layer assignment (0..41) */
  uint32_t depth;               /* Topological depth */
  uint32_t dep_count;           /* Number of dependencies */
  const uint16_t *deps;         /* Dependency indices */
  uint8_t is_critical;          /* Critical path indicator */
} pkg_info_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

/* Initialize dependency graph (loads from manifest or precomputed) */
int dep_resolver_v2_init(dep_graph_v2_t *graph,
                         const char *manifest_path);

/* Get topological depth for package */
uint32_t dep_resolver_v2_get_depth(const dep_graph_v2_t *graph,
                                   uint32_t pkg_idx);

/* Calculate toroidal layer assignment (depth % 42) */
uint32_t dep_resolver_v2_toroidal_layer(const dep_graph_v2_t *graph,
                                        uint32_t pkg_idx);

/* Get package information */
int dep_resolver_v2_get_package_info(const dep_graph_v2_t *graph,
                                     uint32_t pkg_idx,
                                     pkg_info_t *info);

/* Get dependencies for package (CSR row iteration) */
int dep_resolver_v2_get_dependencies(const dep_graph_v2_t *graph,
                                     uint32_t pkg_idx,
                                     uint16_t *deps_out,
                                     uint32_t max_deps);

/* Get all packages in layer */
int dep_resolver_v2_get_layer_packages(const dep_graph_v2_t *graph,
                                       uint32_t layer_idx,
                                       uint32_t *pkg_indices,
                                       uint32_t max_packages);

/* Check if dependencies of package are satisfied */
uint8_t dep_resolver_v2_deps_satisfied(const dep_graph_v2_t *graph,
                                       uint32_t pkg_idx,
                                       const uint64_t *completion_bitmap);

/* Detect if package is on critical path (depth == max depth) */
uint8_t dep_resolver_v2_is_critical_path(const dep_graph_v2_t *graph,
                                         uint32_t pkg_idx);

/* Detect cycles using gcd(depth_i, depth_j) */
int dep_resolver_v2_detect_cycles(const dep_graph_v2_t *graph);

/* Get performance metrics */
typedef struct {
  uint32_t mean_depth;
  uint32_t max_depth;
  uint32_t critical_path_length;
  double mean_deps_per_pkg;
  uint32_t disconnected_components;
} resolver_metrics_t;

int dep_resolver_v2_get_metrics(const dep_graph_v2_t *graph,
                                resolver_metrics_t *metrics);

/* Generate human-readable report */
int dep_resolver_v2_report(const dep_graph_v2_t *graph,
                           char *buffer,
                           size_t buffer_size);

/* Cleanup */
void dep_resolver_v2_free(dep_graph_v2_t *graph);

#endif /* TERMUX_DEP_RESOLVER_V2_H */
