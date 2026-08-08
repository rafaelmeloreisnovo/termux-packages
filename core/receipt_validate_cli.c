/*
 * REAL: CLI for verifying and reporting receipts.
 * Status: REAL — recomputes SHA256 and rejects tampered files.
 *
 * Usage:
 *   receipt-validate <receipt.json>   — verify and report
 *
 * Exit codes:
 *   0 — receipt is valid (signature matches, tamper-free)
 *   1 — signature mismatch or field violation (fail-closed)
 *   2 — I/O or usage error
 */

#include "real_receipt.h"
#include <stdio.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <receipt.json>\n", argv[0]);
    return 2;
  }
  real_receipt_t r;
  int rc = real_receipt_verify_file(argv[1], &r);
  if (rc == 0) {
    printf("✓ receipt %s: SIGNATURE VALID\n", argv[1]);
    real_receipt_report(stdout, &r);
    return 0;
  }
  fprintf(stderr, "✗ receipt %s: SIGNATURE MISMATCH or missing fields\n",
          argv[1]);
  return 1;
}
