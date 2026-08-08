#ifndef PKG_DAG_H
#define PKG_DAG_H

/*
 * Static package dependency projection.
 * Status: OBSERVED_LIMITED.
 *
 * Edges come from the current static parser's DEPENDS/BUILD_DEPENDS extraction.
 * Alternatives such as "A | B" are explicitly counted and represented by the
 * first alternative only; providers, shell conditionals and dynamic Bash
 * evaluation remain outside this projection.
 *
 * Allocation/field-overflow failures are fail-closed. Cycles are reported as
 * cyclic strongly-connected components (SCCs), not as an elementary-cycle
 * enumeration.
 */

#include "pkg_parser.h"
#include "pkg_scanner.h"
#include "real_attrs.h"
#include <stdint.h>
#include <stdio.h>

#define PKG_DAG_MAX_DEPS_PER_PKG 128

typedef struct {
  uint32_t from_idx;
  uint32_t to_idx;
  uint8_t is_build_dep;
} pkg_dag_edge_t;

typedef struct {
  uint32_t pkg_idx;
  char missing_dep[64];
} pkg_dag_unresolved_t;

typedef struct {
  uint32_t *nodes;
  uint32_t length;
} pkg_dag_cycle_t;

typedef struct {
  const pkg_inventory_t *inv;

  pkg_parser_result_t *parsed;
  uint32_t parsed_count;

  pkg_dag_edge_t *edges;
  uint32_t edge_count;
  uint32_t edge_capacity;

  uint32_t **adj;
  uint32_t *adj_len;

  uint32_t *topo_order;
  uint32_t *topo_depth;
  uint32_t topo_count;

  pkg_dag_unresolved_t *unresolved;
  uint32_t unresolved_count;
  uint32_t unresolved_capacity;

  /* Cyclic SCCs. cycle_count is number of cyclic SCCs; cycle_nodes is the
   * total number of nodes participating in those SCCs. */
  pkg_dag_cycle_t *cycles;
  uint32_t cycle_count;
  uint32_t cycle_nodes;

  uint32_t total_depends_edges;
  uint32_t total_build_dep_edges;
  uint32_t max_depth;
  uint32_t parse_failures;

  /* Projection/collection diagnostics. */
  uint32_t alternative_dep_fields;
  uint32_t dependency_field_overflows;
  uint32_t allocation_failures;
} pkg_dag_t;

REAL_HOT REAL_WARN_UNUSED REAL_NONNULL_ALL
int pkg_dag_build(pkg_dag_t *dag, const pkg_inventory_t *inv);

REAL_HOT REAL_WARN_UNUSED REAL_NONNULL_ALL
int pkg_dag_topo_sort(pkg_dag_t *dag);

REAL_COLD REAL_NONNULL_ALL
void pkg_dag_write_json(FILE *out, const pkg_dag_t *dag);

REAL_COLD REAL_NONNULL_ALL
void pkg_dag_report(FILE *out, const pkg_dag_t *dag);

REAL_PURE REAL_NONNULL(1)
const pkg_parser_result_t *pkg_dag_parsed_at(const pkg_dag_t *dag,
                                             uint32_t idx);

void pkg_dag_free(pkg_dag_t *dag);

#endif /* PKG_DAG_H */
