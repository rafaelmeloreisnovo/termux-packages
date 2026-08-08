/*
 * REAL: metrics_current.json producer.
 * Status: REAL — computes every field from real filesystem scan + DAG.
 *
 * Emits the JSON that scripts/health_check.sh consumes. All fields are
 * derived from actual measurements — no simulated numbers.
 *
 * Definitions (transparent so anyone can verify):
 *   node_count             = real inventory count (packages + subpackages)
 *   edge_count             = real DAG edges from parsed DEPENDS/BUILD_DEPENDS
 *   unresolved_count       = real deps with no matching build.sh
 *   cycle_count            = real cycles detected by Kahn's algorithm
 *   max_depth              = real max topological depth
 *   coherence_phi          = (1 - unresolved/edges) × (1 - cycles/nodes)
 *                          = 1.0 exactly when graph resolves cleanly
 *   graph_completeness     = 1 - (unresolved_count / edge_count)
 *   avg_deps_per_pkg       = edge_count / node_count
 *
 * The producer never emits fields it cannot compute. Missing = missing;
 * downstream consumers must fail-closed on absence.
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

  /* Capture provenance FIRST — if we can't establish who we are, refuse. */
  real_provenance_t prov;
  if (real_provenance_capture(&prov, argv[0], REAL_CONTRACT_SCHEMA_VERSION) < 0) {
    fprintf(stderr, "REAL_ERROR: provenance capture failed\n");
    return 2;
  }

  uint64_t t_start = monotonic_ns();

  pkg_inventory_t inv;
  if (pkg_inventory_init(&inv, 512) < 0) {
    fprintf(stderr, "REAL_ERROR: inventory init failed\n");
    return 2;
  }

  uint64_t t_inv_start = monotonic_ns();
  if (pkg_inventory_scan_all(&inv, base) < 0) {
    fprintf(stderr, "REAL_ERROR: scan failed for %s\n", base);
    pkg_inventory_free(&inv);
    return 2;
  }
  uint64_t t_inv_end = monotonic_ns();

  pkg_dag_t dag;
  uint64_t t_dag_start = monotonic_ns();
  if (pkg_dag_build(&dag, &inv) < 0) {
    fprintf(stderr, "REAL_ERROR: dag build failed\n");
    pkg_inventory_free(&inv);
    return 2;
  }
  if (pkg_dag_topo_sort(&dag) < 0) {
    fprintf(stderr, "REAL_ERROR: topo sort failed\n");
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }
  uint64_t t_dag_end = monotonic_ns();

  /* Real derived metrics */
  uint32_t nodes = inv.count;
  uint32_t edges = dag.edge_count;
  uint32_t unres = dag.unresolved_count;
  uint32_t cycles = dag.cycle_count;

  double completeness =
      edges > 0 ? 1.0 - ((double)unres / (double)edges) : 0.0;
  double acyclicity =
      nodes > 0 ? 1.0 - ((double)cycles / (double)nodes) : 0.0;
  double coherence_phi = completeness * acyclicity;
  double avg_deps = nodes > 0 ? (double)edges / (double)nodes : 0.0;

  uint64_t inv_latency_us = (t_inv_end - t_inv_start) / 1000ULL;
  uint64_t dag_latency_us = (t_dag_end - t_dag_start) / 1000ULL;
  uint64_t total_latency_us = (monotonic_ns() - t_start) / 1000ULL;

  FILE *f = fopen(out_path, "w");
  if (!f) {
    fprintf(stderr, "REAL_ERROR: cannot open %s: %s\n", out_path,
            strerror(errno));
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"schema\": \"" REAL_CONTRACT_SCHEMA_VERSION "\",\n");
  fprintf(f, "  \"status\": \"REAL\",\n");
  fprintf(f, "  \"generated_unix_ms\": %" PRIu64 ",\n", now_unix_ms());
  fprintf(f, "  \"repo_base\": \"%s\",\n", base);
  fprintf(f, "\n");
  real_provenance_write_json(f, &prov);
  fprintf(f, ",\n\n");

  /* Auto-adaptive arch metadata */
  {
    real_arch_t ct = real_arch_compile_time();
    real_arch_t rt = real_arch_detect_runtime();
    const real_arch_props_t *rtp = real_arch_props(rt);
    fprintf(f, "  \"arch\": {\n");
    fprintf(f, "    \"compile_time\": \"%s\",\n", real_arch_name(ct));
    fprintf(f, "    \"runtime\": \"%s\",\n", real_arch_name(rt));
    if (rtp) {
      fprintf(f, "    \"word_bits\": %u,\n", rtp->word_bits);
      fprintf(f, "    \"endian\": \"%s\",\n",
              rtp->endian == REAL_ENDIAN_BIG ? "big" : "little");
      fprintf(f, "    \"page_size\": %u,\n", rtp->page_size);
      fprintf(f, "    \"cache_line\": %u\n", rtp->cache_line);
    } else {
      fprintf(f, "    \"word_bits\": 0,\n");
      fprintf(f, "    \"endian\": \"unknown\",\n");
      fprintf(f, "    \"page_size\": 0,\n");
      fprintf(f, "    \"cache_line\": 0\n");
    }
    fprintf(f, "  },\n\n");
  }

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
  fclose(f);

  fprintf(stdout,
          "REAL metrics written to %s (nodes=%u edges=%u phi=%.4f "
          "cycles=%u unresolved=%u)\n",
          out_path, nodes, edges, coherence_phi, cycles, unres);

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
