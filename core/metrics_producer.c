/*
 * pkg_metrics/1.0.0 measurement producer.
 * Status: REAL for the emitted measurement artifact only.
 *
 * Scope boundary:
 *   This producer measures a static FIRST-ALTERNATIVE dependency projection
 *   from the current repository filesystem. It does NOT prove full Bash/apt
 *   dependency semantics, package buildability, Android runtime, physical
 *   device execution, security, or product readiness.
 *
 * Promotion prerequisites enforced here:
 *   - provenance capture succeeds;
 *   - all four known package roots are present and scanner collection has zero
 *     path/I/O/allocation/subpackage failures;
 *   - DAG construction/topological analysis succeeds without allocation or
 *     dependency-field truncation;
 *   - no parser file failures are tolerated.
 *
 * Usage: metrics-producer <repo_base_dir> <output_json_path>
 */

#include "pkg_dag.h"
#include "pkg_parser.h"
#include "pkg_scanner.h"
#include "real_arch.h"
#include "real_contract.h"
#include "real_provenance.h"
#include "real_receipt.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_unix_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
}

static uint64_t monotonic_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <repo_base_dir> <output_json_path>\n", argv[0]);
    return 1;
  }
  const char *base = argv[1];
  const char *out_path = argv[2];

  /* Begin receipt at the very start of the operation. */
  real_receipt_t receipt;
  int have_receipt = (real_receipt_begin(&receipt, "produce_metrics",
                                          argv[0]) == 0);

  real_provenance_t prov;
  if (real_provenance_capture(&prov, argv[0], REAL_CONTRACT_SCHEMA_VERSION) < 0) {
    fprintf(stderr, "BLOCKED: provenance capture failed\n");
    return 2;
  }

  uint64_t t_start = monotonic_ns();

  pkg_inventory_t inv;
  if (pkg_inventory_init(&inv, 512) < 0) {
    fprintf(stderr, "BLOCKED: inventory init failed\n");
    return 2;
  }

  uint64_t t_inv_start = monotonic_ns();
  if (pkg_inventory_scan_all(&inv, base) < 0) {
    fprintf(stderr,
            "BLOCKED: inventory collection failed for %s "
            "(roots=%u/%u absent=%u failed=%u path=%u io=%u alloc=%u subpkg=%u)\n",
            base, inv.roots_present, inv.roots_expected, inv.roots_absent,
            inv.roots_failed, inv.path_errors, inv.io_errors,
            inv.allocation_errors, inv.subpackage_scan_failures);
    pkg_inventory_free(&inv);
    return 2;
  }
  uint64_t t_inv_end = monotonic_ns();

  if (!pkg_inventory_is_complete(&inv)) {
    fprintf(stderr,
            "BLOCKED: inventory coverage incomplete "
            "(roots=%u/%u absent=%u failed=%u path=%u io=%u alloc=%u subpkg=%u)\n",
            inv.roots_present, inv.roots_expected, inv.roots_absent,
            inv.roots_failed, inv.path_errors, inv.io_errors,
            inv.allocation_errors, inv.subpackage_scan_failures);
    pkg_inventory_free(&inv);
    return 2;
  }

  pkg_dag_t dag;
  uint64_t t_dag_start = monotonic_ns();
  if (pkg_dag_build(&dag, &inv) < 0) {
    fprintf(stderr,
            "BLOCKED: dag build failed (alloc=%u field_overflows=%u)\n",
            dag.allocation_failures, dag.dependency_field_overflows);
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }

  if (dag.parse_failures != 0) {
    fprintf(stderr, "BLOCKED: parser file failures=%u\n", dag.parse_failures);
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }

  if (pkg_dag_topo_sort(&dag) < 0) {
    fprintf(stderr, "BLOCKED: topo/SCC analysis failed (alloc=%u)\n",
            dag.allocation_failures);
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }
  uint64_t t_dag_end = monotonic_ns();

  uint32_t nodes = inv.count;
  uint32_t edges = dag.edge_count;
  uint32_t unres = dag.unresolved_count;
  uint32_t cycles = dag.cycle_count;

  /* Legacy contract field name retained, but semantics are explicitly scoped:
   * fraction of projected dependency names resolved to inventory entries. */
  double completeness =
      edges > 0 ? 1.0 - ((double)unres / (double)edges) : 0.0;

  /* Acyclicity is a graph property, not 1 - SCC_count/node_count. */
  double acyclicity = nodes > 0 ? (cycles == 0 ? 1.0 : 0.0) : 0.0;
  double coherence_phi = completeness * acyclicity;
  double avg_deps = nodes > 0 ? (double)edges / (double)nodes : 0.0;

  uint64_t inv_latency_us = (t_inv_end - t_inv_start) / 1000ULL;
  uint64_t dag_latency_us = (t_dag_end - t_dag_start) / 1000ULL;
  uint64_t total_latency_us = (monotonic_ns() - t_start) / 1000ULL;

  FILE *f = fopen(out_path, "w");
  if (!f) {
    fprintf(stderr, "BLOCKED: cannot open %s: %s\n", out_path,
            strerror(errno));
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"schema\": \"" REAL_CONTRACT_SCHEMA_VERSION "\",\n");
  fprintf(f, "  \"status\": \"REAL\",\n");
  fprintf(f, "  \"claim_allowed\": false,\n");
  fprintf(f, "  \"measurement_scope\": \"static_first_alternative_dependency_projection\",\n");
  fprintf(f, "  \"graph_completeness_semantics\": \"resolved_name_fraction_of_projection_edges\",\n");
  fprintf(f, "  \"cycle_semantics\": \"cyclic_scc_count\",\n");
  fprintf(f, "  \"product_readiness\": \"NOT_CLAIMED\",\n");
  fprintf(f, "  \"device_runtime\": \"NOT_MEASURED_SEPARATE_RECEIPT_REQUIRED\",\n");
  fprintf(f, "  \"generated_unix_ms\": %" PRIu64 ",\n", now_unix_ms());
  fprintf(f, "  \"repo_base\": \"%s\",\n", base);
  fprintf(f, "\n");
  real_provenance_write_json(f, &prov);
  fprintf(f, ",\n\n");

  {
    real_arch_t ct = real_arch_compile_time();
    real_arch_t rt = real_arch_detect_runtime();
    fprintf(f, "  \"arch\": {\n");
    fprintf(f, "    \"scope\": \"identity_only\",\n");
    fprintf(f, "    \"compile_time\": \"%s\",\n", real_arch_name(ct));
    fprintf(f, "    \"runtime\": \"%s\",\n", real_arch_name(rt));
    fprintf(f, "    \"capability_evidence\": \"arch_runtime_probe/1.0.0\"\n");
    fprintf(f, "  },\n\n");
  }

  fprintf(f, "  \"inventory_coverage\": {\n");
  fprintf(f, "    \"complete\": true,\n");
  fprintf(f, "    \"roots_expected\": %u,\n", inv.roots_expected);
  fprintf(f, "    \"roots_present\": %u,\n", inv.roots_present);
  fprintf(f, "    \"roots_absent\": %u,\n", inv.roots_absent);
  fprintf(f, "    \"roots_failed\": %u,\n", inv.roots_failed);
  fprintf(f, "    \"path_errors\": %u,\n", inv.path_errors);
  fprintf(f, "    \"io_errors\": %u,\n", inv.io_errors);
  fprintf(f, "    \"allocation_errors\": %u,\n", inv.allocation_errors);
  fprintf(f, "    \"subpackage_scan_failures\": %u\n", inv.subpackage_scan_failures);
  fprintf(f, "  },\n");
  fprintf(f, "  \"parse_failures\": %u,\n", dag.parse_failures);
  fprintf(f, "  \"alternative_dep_fields\": %u,\n", dag.alternative_dep_fields);
  fprintf(f, "  \"dependency_field_overflows\": %u,\n", dag.dependency_field_overflows);
  fprintf(f, "  \"dag_allocation_failures\": %u,\n", dag.allocation_failures);
  fprintf(f, "  \"cycle_nodes\": %u,\n\n", dag.cycle_nodes);

  fprintf(f, "  \"node_count\": %u,\n", nodes);
  fprintf(f, "  \"edge_count\": %u,\n", edges);
  fprintf(f, "  \"depends_edges\": %u,\n", dag.total_depends_edges);
  fprintf(f, "  \"build_dep_edges\": %u,\n", dag.total_build_dep_edges);
  fprintf(f, "  \"unresolved_count\": %u,\n", unres);
  fprintf(f, "  \"cycle_count\": %u,\n", cycles);
  fprintf(f, "  \"max_depth\": %u,\n", dag.max_depth);
  fprintf(f, "  \"topo_ordered\": %u,\n", dag.topo_count);
  fprintf(f, "\n");
  fprintf(f, "  \"coherence_phi\": %.6f,\n", coherence_phi);
  fprintf(f, "  \"graph_completeness\": %.6f,\n", completeness);
  fprintf(f, "  \"graph_acyclicity\": %.6f,\n", acyclicity);
  fprintf(f, "  \"avg_deps_per_pkg\": %.6f,\n", avg_deps);
  fprintf(f, "\n");
  fprintf(f, "  \"inventory_latency_us\": %" PRIu64 ",\n", inv_latency_us);
  fprintf(f, "  \"dag_latency_us\": %" PRIu64 ",\n", dag_latency_us);
  fprintf(f, "  \"total_latency_us\": %" PRIu64 "\n", total_latency_us);
  fprintf(f, "}\n");

  /* B11 fix: check ferror() BEFORE fclose so ENOSPC/EIO during any of
   * the preceding fprintfs is surfaced instead of quietly leaving a
   * truncated metrics.json on disk. Contract-validate would catch the
   * truncation downstream, but fail-here is much clearer. */
  int stream_err = ferror(f);
  if (fclose(f) != 0 || stream_err) {
    fprintf(stderr, "BLOCKED: write/close failed for %s (stream_err=%d)\n",
            out_path, stream_err);
    unlink(out_path);
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }

  fprintf(stdout,
          "pkg_metrics written to %s scope=static_first_alternative_dependency_projection "
          "nodes=%u edges=%u phi=%.4f cyclic_sccs=%u cycle_nodes=%u unresolved=%u "
          "alternatives=%u roots=%u/%u parse_failures=%u claim_allowed=false\n",
          out_path, nodes, edges, coherence_phi, cycles, dag.cycle_nodes, unres,
          dag.alternative_dep_fields, inv.roots_present, inv.roots_expected,
          dag.parse_failures);

  /* Emit receipt alongside the JSON output (path.receipt). */
  if (have_receipt) {
    if (real_receipt_add_output(&receipt, out_path) == 0 &&
        real_receipt_seal(&receipt, 0) == 0) {
      char rcpt_path[512];
      snprintf(rcpt_path, sizeof(rcpt_path), "%s.receipt", out_path);
      if (real_receipt_write(&receipt, rcpt_path) == 0) {
        fprintf(stdout, "REAL receipt sealed: %s (sha=%.16s...)\n",
                rcpt_path, receipt.content_sha256_hex);
      } else {
        fprintf(stderr, "REAL_WARN: could not write receipt to %s\n",
                rcpt_path);
      }
    }
  }

  pkg_dag_free(&dag);
  pkg_inventory_free(&inv);
  return 0;
}
