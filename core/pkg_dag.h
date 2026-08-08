#ifndef PKG_DAG_H
#define PKG_DAG_H

/*
 * REAL: Dependency graph and DAG construction
 * Status: REAL — computed from real parsed data, no simulation.
 *
 * Items #3, #4, #5 from consolidation list:
 *   - Real dependency extraction (parse depends_raw into edges)
 *   - Real DAG construction (adjacency lists over inventory)
 *   - Real cycle detection and unresolved-dep reporting (TOKEN_VAZIO)
 */

#include "pkg_scanner.h"
#include "pkg_parser.h"
#include <stdint.h>
#include <stdio.h>

#define PKG_DAG_MAX_DEPS_PER_PKG 128

typedef struct {
  uint32_t from_idx;              /* index into inventory */
  uint32_t to_idx;                /* index into inventory */
  uint8_t is_build_dep;           /* 1 if from BUILD_DEPENDS, 0 if DEPENDS */
} pkg_dag_edge_t;

typedef struct {
  uint32_t pkg_idx;               /* which package failed to resolve */
  char missing_dep[64];           /* dependency name that has no build.sh */
} pkg_dag_unresolved_t;

typedef struct {
  uint32_t *nodes;                /* cycle path (indices) */
  uint32_t length;
} pkg_dag_cycle_t;

typedef struct {
  /* Inventory (borrowed reference, not owned) */
  const pkg_inventory_t *inv;

  /* Parsed package data (owned): parallel array with inv->entries */
  pkg_parser_result_t *parsed;
  uint32_t parsed_count;

  /* Edges (adjacency in COO form for simplicity + adjacency lists) */
  pkg_dag_edge_t *edges;
  uint32_t edge_count;
  uint32_t edge_capacity;

  /* Adjacency list: for each package, list of dep indices */
  uint32_t **adj;                 /* adj[i] = array of pkg indices */
  uint32_t *adj_len;              /* adj_len[i] = count */

  /* Topological order (output of topo sort) */
  uint32_t *topo_order;           /* pkg indices in build order */
  uint32_t *topo_depth;           /* depth per pkg (0 = leaf) */
  uint32_t topo_count;            /* == inv->count if fully acyclic */

  /* Unresolved / TOKEN_VAZIO diagnostics */
  pkg_dag_unresolved_t *unresolved;
  uint32_t unresolved_count;
  uint32_t unresolved_capacity;

  /* Cycles (only populated if cycles found) */
  pkg_dag_cycle_t *cycles;
  uint32_t cycle_count;

  /* Stats */
  uint32_t total_depends_edges;
  uint32_t total_build_dep_edges;
  uint32_t max_depth;
} pkg_dag_t;

/* Initialize DAG using the given inventory (not owned). Parses every build.sh. */
int pkg_dag_build(pkg_dag_t *dag, const pkg_inventory_t *inv);

/* Compute topological order and depths. Fills unresolved[] / cycles[]. */
int pkg_dag_topo_sort(pkg_dag_t *dag);

/* Write summary + counts as JSON. */
void pkg_dag_write_json(FILE *out, const pkg_dag_t *dag);

/* Human report. */
void pkg_dag_report(FILE *out, const pkg_dag_t *dag);

/* Get parsed data for a package by inventory index. */
const pkg_parser_result_t *pkg_dag_parsed_at(const pkg_dag_t *dag,
                                             uint32_t idx);

void pkg_dag_free(pkg_dag_t *dag);

#endif /* PKG_DAG_H */
