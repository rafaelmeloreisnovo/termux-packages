/*
 * REAL: CLI to validate a metrics JSON against pkg_metrics/1.0.0.
 * Status: REAL — reads real file, applies real invariants, exits fail-closed.
 *
 * Exit codes:
 *   0 — contract satisfied (all invariants hold)
 *   1 — contract violated (details on stderr)
 *   2 — usage / I/O error
 */

#include "real_contract.h"
#include "real_provenance.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <metrics.json>\n", argv[0]);
    return 2;
  }

  real_contract_v1_t out;
  real_contract_report_t rep;
  int rc = real_contract_validate_file(argv[1], &out, &rep);

  real_contract_report_print(rc == 0 ? stdout : stderr, &rep);

  if (rc == 0) {
    printf("Validated fields:\n");
    printf("  schema       = %s\n", out.provenance_schema);
    printf("  git_commit   = %s\n", out.provenance_git_commit);
    printf("  toolchain    = %s\n", out.provenance_toolchain);
    printf("  producer     = %s\n", out.provenance_producer);
    printf("  node_count   = %u\n", out.node_count);
    printf("  edge_count   = %u\n", out.edge_count);
    printf("  coherence_phi = %.6f\n", out.coherence_phi);
    printf("  cycle_count  = %u\n", out.cycle_count);
    return 0;
  }
  return 1;
}
