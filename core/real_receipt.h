#ifndef REAL_RECEIPT_H
#define REAL_RECEIPT_H

/*
 * REAL: signed receipts for every operation.
 * Status: REAL — every field computed from real sources; SHA256 signs
 * the whole receipt content.
 *
 * Contract: receipt_v1
 *
 * A receipt records ONE operation:
 *   - what was done (operation name)
 *   - who did it (provenance: git SHA, toolchain, host, arch)
 *   - what went in  (inputs: file paths + SHA256 + size)
 *   - what came out (outputs: file paths + SHA256 + size)
 *   - how it ended  (exit_code, duration_us)
 *   - signature     (SHA256 of the receipt content, computed last)
 *
 * The signature makes the receipt tamper-evident: any edit changes the
 * content hash, and a verifier recomputes it and rejects on mismatch.
 *
 * Enables:
 *   #49 (receipts per build/device/benchmark)
 *   #52 (baseline comparable) — receipts are the baseline
 *   #53 (byte-a-byte reproducibility) — receipt SHAs match iff bytes match
 *   #54 (provenance per SHA/commit/toolchain) — embedded in every receipt
 */

#include "real_attrs.h"
#include "real_provenance.h"
#include "real_sha256.h"
#include <stdint.h>
#include <stdio.h>

#define REAL_RECEIPT_SCHEMA "receipt/1.0.0"

#define RECEIPT_MAX_IO   16     /* max inputs OR outputs per receipt */
#define RECEIPT_PATH_MAX 256
#define RECEIPT_OP_MAX   64

typedef struct {
  char path[RECEIPT_PATH_MAX];
  char sha256_hex[REAL_SHA256_HEXLEN];  /* 64 hex + NUL */
  uint64_t size_bytes;
} real_receipt_io_t;

typedef struct {
  char operation[RECEIPT_OP_MAX];  /* e.g. "produce_metrics" */
  real_provenance_t provenance;
  char arch_compile[32];
  char arch_runtime[32];

  real_receipt_io_t inputs[RECEIPT_MAX_IO];
  uint32_t input_count;
  real_receipt_io_t outputs[RECEIPT_MAX_IO];
  uint32_t output_count;

  int32_t  exit_code;
  uint64_t duration_us;
  uint64_t started_unix_ms;
  uint64_t finished_unix_ms;

  /* Computed by seal — SHA256 over the canonical form of every field
   * above (excluding this field itself). */
  char content_sha256_hex[REAL_SHA256_HEXLEN];
} real_receipt_t;

/* Zero out and set operation + provenance. Call once at start. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_receipt_begin(real_receipt_t *r, const char *operation,
                       const char *argv0);

/* Add an input file to the receipt; hashes it now. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_receipt_add_input(real_receipt_t *r, const char *path);

/* Add an output file; hashes it now. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_receipt_add_output(real_receipt_t *r, const char *path);

/* Record duration + exit code + compute content_sha256. */
REAL_WARN_UNUSED REAL_NONNULL(1)
int real_receipt_seal(real_receipt_t *r, int exit_code);

/* Write receipt JSON to path. Requires seal() to have been called. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_receipt_write(const real_receipt_t *r, const char *path);

/* Parse + verify a receipt from disk. Recomputes content SHA and
 * rejects if mismatched (tamper detection). Returns 0 if valid. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_receipt_verify_file(const char *path, real_receipt_t *out);

/* Human report to fp. */
REAL_COLD REAL_NONNULL_ALL
void real_receipt_report(FILE *fp, const real_receipt_t *r);

#endif /* REAL_RECEIPT_H */
