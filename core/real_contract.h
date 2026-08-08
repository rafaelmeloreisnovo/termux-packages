#ifndef REAL_CONTRACT_H
#define REAL_CONTRACT_H

/*
 * REAL: contract definitions for output JSON.
 * Status: REAL — declares invariants and validates output against them.
 *
 * The contract is the single source of truth for what a REAL metrics
 * output MUST contain. Every field is declared here with its type and
 * (where meaningful) numeric bounds. The validator rejects any output
 * that lacks a required field, contains TOKEN_VAZIO, or violates a
 * bound.
 *
 * Contract version: pkg_metrics/1.0.0
 *
 * Guarantee: a JSON that passes validation contains ONLY REAL data
 * (no simulated values, no missing fields, no placeholder strings).
 */

#include "real_attrs.h"
#include <stdint.h>
#include <stdio.h>

#define REAL_CONTRACT_SCHEMA_VERSION "pkg_metrics/1.0.0"

/* All numeric fields that must be present in a REAL metrics JSON.
 * ptr fields are populated by validator; caller passes zeroed struct. */
typedef struct {
  /* Counters (uint32) — all required, all >= 0. */
  uint32_t node_count;
  uint32_t edge_count;
  uint32_t depends_edges;
  uint32_t build_dep_edges;
  uint32_t unresolved_count;
  uint32_t cycle_count;
  uint32_t max_depth;
  uint32_t topo_ordered;

  /* Doubles (bounded to [0.0, 1.0] for the phi family). */
  double coherence_phi;
  double graph_completeness;
  double graph_acyclicity;
  double avg_deps_per_pkg;

  /* Latencies (uint64, microseconds) — must be > 0. */
  uint64_t inventory_latency_us;
  uint64_t dag_latency_us;
  uint64_t total_latency_us;

  /* Provenance block — every field non-empty, no TOKEN_VAZIO. */
  char provenance_git_commit[64];
  char provenance_build_ts[64];
  char provenance_toolchain[128];
  char provenance_producer[64];
  char provenance_host[192];
  char provenance_schema[64];
  uint64_t provenance_run_ms;
} real_contract_v1_t;

/* Violation record produced by validator. */
typedef struct {
  char field[64];
  char reason[192];
} real_contract_violation_t;

typedef struct {
  real_contract_violation_t items[32];
  uint32_t count;
} real_contract_report_t;

/* Parse and validate a JSON file against pkg_metrics/1.0.0.
 * Populates *out on success, *report always with any violations found.
 * Returns 0 iff report->count == 0 (all invariants satisfied).
 * Returns -1 on I/O failure or malformed JSON (which is itself a
 * violation and gets reported). */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_contract_validate_file(const char *json_path,
                                real_contract_v1_t *out,
                                real_contract_report_t *report);

/* Human-readable violation report. */
REAL_COLD REAL_NONNULL_ALL
void real_contract_report_print(FILE *fp, const real_contract_report_t *r);

#endif /* REAL_CONTRACT_H */
