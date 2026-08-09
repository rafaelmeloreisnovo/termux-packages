#ifndef REAL_LEDGER_H
#define REAL_LEDGER_H

/*
 * REAL: append-only chain-of-custody ledger.
 * Status: REAL — every entry sealed with SHA256 and linked to the prior
 * entry's hash. Tampering with entry N invalidates every entry >= N.
 *
 * Contract: ledger_entry/1.0.0
 *
 * File format: JSONL (one JSON object per line). Order matters — each
 * entry references the previous entry's tail hash. A fresh ledger has
 * no prev_tail_sha256 (empty string).
 *
 * Entry canonical form (bytes fed to SHA256 for entry_sha256):
 *   seq=<N>\n
 *   prev_tail=<prev_entry_sha256_or_empty>\n
 *   receipt_sha=<receipt.content_sha256>\n
 *   receipt_path=<absolute_or_relative_path>\n
 *   appended_unix_ms=<T>\n
 *
 * entry_sha256 becomes the next entry's prev_tail. Chain-hash guarantees:
 *   - deleting entry N shifts every subsequent seq, breaking every hash
 *   - editing entry N-in-place changes entry_sha256, so N+1's prev_tail
 *     no longer matches the recomputed N.entry_sha256
 *   - re-hashing all N..end also requires knowing all subsequent receipt
 *     SHAs, so forgery still requires access to every downstream artifact
 *
 * This is deliberately NOT a cryptographic signature (no private key).
 * Provenance is by construction: every field is independently verifiable
 * against real files on disk.
 */

#include "real_attrs.h"
#include "real_receipt.h"
#include "real_sha256.h"
#include <stdint.h>
#include <stdio.h>

#define REAL_LEDGER_SCHEMA "ledger_entry/1.0.0"

#define LEDGER_PATH_MAX  512

typedef struct {
  uint64_t seq;
  char prev_tail_sha256_hex[REAL_SHA256_HEXLEN];  /* empty for seq=0 */
  char receipt_sha256_hex[REAL_SHA256_HEXLEN];
  char receipt_path[LEDGER_PATH_MAX];
  uint64_t appended_unix_ms;
  char entry_sha256_hex[REAL_SHA256_HEXLEN];  /* computed by seal */
} real_ledger_entry_t;

/* Read the last entry (if any) and set out_prev_tail (empty if fresh
 * ledger). Also fills out_next_seq. Returns 0 on success (even for empty
 * ledger — in which case entry_count=0). Returns -1 on I/O error. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_ledger_tail(const char *ledger_path,
                     char out_prev_tail[REAL_SHA256_HEXLEN],
                     uint64_t *out_next_seq,
                     uint64_t *out_entry_count);

/* Append a receipt to the ledger. Verifies the receipt (via
 * real_receipt_verify_file) before appending. Computes entry_sha256 from
 * canonical form. Returns 0 on success. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_ledger_append(const char *ledger_path, const char *receipt_path);

/* Walk the ledger, verifying every entry: chain-hash intact, each
 * receipt file present and its content_sha256 matches the recorded
 * receipt_sha256, and the entry_sha256 itself recomputes correctly.
 * Fills *out_entries_verified. Returns 0 iff every entry checks out. */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_ledger_verify(const char *ledger_path,
                       uint64_t *out_entries_verified);

/* Human summary printed to fp. */
REAL_COLD REAL_NONNULL_ALL
void real_ledger_report(FILE *fp, const char *ledger_path);

#endif /* REAL_LEDGER_H */
