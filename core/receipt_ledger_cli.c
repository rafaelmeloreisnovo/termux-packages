/*
 * REAL: chain-of-custody ledger CLI.
 * Status: REAL — every operation touches real files and real SHA256s.
 *
 * Usage:
 *   receipt-ledger append <ledger.jsonl> <receipt.json>
 *   receipt-ledger verify <ledger.jsonl>
 *   receipt-ledger tail   <ledger.jsonl>
 *
 * Exit codes:
 *   0 — success
 *   1 — verification failed (chain broken, tampered receipt, missing file)
 *   2 — usage / I/O error
 */

#include "real_ledger.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int usage(void) {
  fprintf(stderr,
          "Usage:\n"
          "  receipt-ledger append <ledger.jsonl> <receipt.json>\n"
          "  receipt-ledger verify <ledger.jsonl>\n"
          "  receipt-ledger tail   <ledger.jsonl>\n");
  return 2;
}

int main(int argc, char **argv) {
  if (argc < 3) return usage();

  if (strcmp(argv[1], "append") == 0) {
    if (argc < 4) return usage();
    if (real_ledger_append(argv[2], argv[3]) != 0) {
      fprintf(stderr, "✗ append FAILED (invalid receipt or I/O error)\n");
      return 1;
    }
    char tail[REAL_SHA256_HEXLEN];
    uint64_t next_seq = 0, count = 0;
    if (real_ledger_tail(argv[2], tail, &next_seq, &count) != 0) {
      fprintf(stderr, "✗ appended but could not re-read tail\n");
      return 1;
    }
    printf("✓ appended receipt: %s\n", argv[3]);
    printf("  ledger:   %s\n", argv[2]);
    printf("  entries:  %" PRIu64 "\n", count);
    printf("  tail:     %s\n", tail);
    return 0;
  }

  if (strcmp(argv[1], "verify") == 0) {
    uint64_t verified = 0;
    int rc = real_ledger_verify(argv[2], &verified);
    if (rc == 0) {
      printf("✓ ledger %s: CHAIN INTACT (%" PRIu64 " entries verified)\n",
             argv[2], verified);
      real_ledger_report(stdout, argv[2]);
      return 0;
    }
    fprintf(stderr,
            "✗ ledger %s: CHAIN BROKEN after %" PRIu64 " good entries\n",
            argv[2], verified);
    return 1;
  }

  if (strcmp(argv[1], "tail") == 0) {
    real_ledger_report(stdout, argv[2]);
    return 0;
  }
  return usage();
}
