#include "real_ledger.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * REAL: append-only ledger with SHA256 chain-hash.
 * ============================================================================ */

static uint64_t now_unix_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
}

/* Canonical serialize the fields that go into entry_sha256. */
static void entry_canonical(const real_ledger_entry_t *e,
                            real_sha256_ctx_t *h) {
  #define UPD(s) real_sha256_update(h, (const uint8_t *)(s), strlen(s))
  char nb[32];
  UPD("seq=");             snprintf(nb, sizeof(nb), "%" PRIu64, e->seq);
                           UPD(nb); UPD("\n");
  UPD("prev_tail=");       UPD(e->prev_tail_sha256_hex[0]
                                   ? e->prev_tail_sha256_hex : "");
                           UPD("\n");
  UPD("receipt_sha=");     UPD(e->receipt_sha256_hex);       UPD("\n");
  UPD("receipt_path=");    UPD(e->receipt_path);             UPD("\n");
  UPD("appended_unix_ms=");snprintf(nb, sizeof(nb), "%" PRIu64,
                                    e->appended_unix_ms);
                           UPD(nb); UPD("\n");
  #undef UPD
}

static void seal_entry(real_ledger_entry_t *e) {
  real_sha256_ctx_t h;
  real_sha256_init(&h);
  entry_canonical(e, &h);
  uint8_t digest[REAL_SHA256_DIGEST];
  real_sha256_final(&h, digest);
  real_sha256_hex(digest, e->entry_sha256_hex);
}

/* Parse one entry line (JSON-ish flat). Returns 1 on success, 0 on EOF,
 * -1 on parse failure. */
static int parse_entry_line(const char *line, real_ledger_entry_t *e) {
  memset(e, 0, sizeof(*e));

  /* Very small hand parser — line is trusted to be what we wrote. */
  const char *p;
  #define FIND(k) ((p = strstr(line, "\"" k "\":")) ? p + strlen("\"" k "\":") : NULL)

  const char *q;
  #define GRAB_U64(k, dst) do { \
      q = FIND(k); if (!q) return -1; \
      while (*q == ' ' || *q == '"') q++; \
      dst = strtoull(q, NULL, 10); \
    } while (0)

  #define GRAB_STR(k, dst, cap) do { \
      q = FIND(k); if (!q) return -1; \
      while (*q == ' ') q++; \
      if (*q != '"') return -1; \
      q++; size_t i = 0; \
      while (*q && *q != '"' && i + 1 < (cap)) (dst)[i++] = *q++; \
      (dst)[i] = '\0'; \
    } while (0)

  GRAB_U64("seq", e->seq);
  GRAB_STR("prev_tail_sha256", e->prev_tail_sha256_hex,
           sizeof(e->prev_tail_sha256_hex));
  GRAB_STR("receipt_sha256",   e->receipt_sha256_hex,
           sizeof(e->receipt_sha256_hex));
  GRAB_STR("receipt_path",     e->receipt_path,
           sizeof(e->receipt_path));
  GRAB_U64("appended_unix_ms", e->appended_unix_ms);
  GRAB_STR("entry_sha256",     e->entry_sha256_hex,
           sizeof(e->entry_sha256_hex));

  #undef FIND
  #undef GRAB_U64
  #undef GRAB_STR
  return 1;
}

static void write_entry_line(FILE *out, const real_ledger_entry_t *e) {
  fprintf(out,
          "{\"schema\":\"" REAL_LEDGER_SCHEMA "\","
          "\"seq\":%" PRIu64 ","
          "\"prev_tail_sha256\":\"%s\","
          "\"receipt_sha256\":\"%s\","
          "\"receipt_path\":\"%s\","
          "\"appended_unix_ms\":%" PRIu64 ","
          "\"entry_sha256\":\"%s\"}\n",
          e->seq, e->prev_tail_sha256_hex, e->receipt_sha256_hex,
          e->receipt_path, e->appended_unix_ms, e->entry_sha256_hex);
}

int real_ledger_tail(const char *ledger_path,
                     char out_prev_tail[REAL_SHA256_HEXLEN],
                     uint64_t *out_next_seq,
                     uint64_t *out_entry_count) {
  /* NONNULL_ALL */
  out_prev_tail[0] = '\0';
  *out_next_seq = 0;
  *out_entry_count = 0;

  FILE *f = fopen(ledger_path, "r");
  if (!f) {
    if (errno == ENOENT) return 0;   /* empty ledger is OK */
    return -1;
  }

  char line[2048];
  real_ledger_entry_t last;
  int have_any = 0;
  while (fgets(line, sizeof(line), f)) {
    real_ledger_entry_t e;
    if (parse_entry_line(line, &e) != 1) { fclose(f); return -1; }
    last = e;
    have_any = 1;
    (*out_entry_count)++;
  }
  fclose(f);
  if (have_any) {
    strncpy(out_prev_tail, last.entry_sha256_hex, REAL_SHA256_HEXLEN - 1);
    out_prev_tail[REAL_SHA256_HEXLEN - 1] = '\0';
    *out_next_seq = last.seq + 1;
  }
  return 0;
}

int real_ledger_append(const char *ledger_path, const char *receipt_path) {
  /* NONNULL_ALL */

  /* Verify the receipt is valid + signature intact before appending. */
  real_receipt_t r;
  if (real_receipt_verify_file(receipt_path, &r) != 0) return -1;

  char prev_tail[REAL_SHA256_HEXLEN];
  uint64_t next_seq = 0;
  uint64_t count = 0;
  if (real_ledger_tail(ledger_path, prev_tail, &next_seq, &count) != 0)
    return -1;

  real_ledger_entry_t e;
  memset(&e, 0, sizeof(e));
  e.seq = next_seq;
  strncpy(e.prev_tail_sha256_hex, prev_tail, sizeof(e.prev_tail_sha256_hex) - 1);
  strncpy(e.receipt_sha256_hex, r.content_sha256_hex,
          sizeof(e.receipt_sha256_hex) - 1);
  strncpy(e.receipt_path, receipt_path, sizeof(e.receipt_path) - 1);
  e.appended_unix_ms = now_unix_ms();
  seal_entry(&e);

  FILE *f = fopen(ledger_path, "a");
  if (!f) return -1;
  write_entry_line(f, &e);
  int err = fflush(f);
  fclose(f);
  return err == 0 ? 0 : -1;
}

int real_ledger_verify(const char *ledger_path,
                       uint64_t *out_entries_verified) {
  /* NONNULL_ALL */
  *out_entries_verified = 0;

  FILE *f = fopen(ledger_path, "r");
  if (!f) return -1;

  char line[2048];
  char expected_prev[REAL_SHA256_HEXLEN] = {0};
  uint64_t expected_seq = 0;
  int chain_ok = 1;

  while (fgets(line, sizeof(line), f)) {
    real_ledger_entry_t e;
    if (parse_entry_line(line, &e) != 1) { chain_ok = 0; break; }

    /* seq must be sequential */
    if (e.seq != expected_seq) { chain_ok = 0; break; }

    /* prev_tail must match previous entry's entry_sha256 (or empty on seq 0) */
    if (strcmp(e.prev_tail_sha256_hex, expected_prev) != 0) {
      chain_ok = 0; break;
    }

    /* Recompute entry_sha256 and compare */
    char stored_sha[REAL_SHA256_HEXLEN];
    strncpy(stored_sha, e.entry_sha256_hex, sizeof(stored_sha) - 1);
    stored_sha[sizeof(stored_sha) - 1] = '\0';
    seal_entry(&e);
    if (strcmp(stored_sha, e.entry_sha256_hex) != 0) {
      chain_ok = 0; break;
    }

    /* Verify the referenced receipt file is present + intact + matches
     * the recorded receipt_sha256. */
    real_receipt_t r;
    if (real_receipt_verify_file(e.receipt_path, &r) != 0) {
      chain_ok = 0; break;
    }
    if (strcmp(r.content_sha256_hex, e.receipt_sha256_hex) != 0) {
      chain_ok = 0; break;
    }

    /* Advance */
    strncpy(expected_prev, e.entry_sha256_hex, sizeof(expected_prev) - 1);
    expected_prev[sizeof(expected_prev) - 1] = '\0';
    expected_seq = e.seq + 1;
    (*out_entries_verified)++;
  }

  fclose(f);
  return chain_ok ? 0 : -1;
}

void real_ledger_report(FILE *fp, const char *ledger_path) {
  /* NONNULL_ALL */
  char tail[REAL_SHA256_HEXLEN];
  uint64_t next_seq = 0, count = 0;
  if (real_ledger_tail(ledger_path, tail, &next_seq, &count) != 0) {
    fprintf(fp, "=== REAL Ledger: %s (READ_ERROR) ===\n", ledger_path);
    return;
  }
  fprintf(fp, "=== REAL Ledger: %s ===\n", ledger_path);
  fprintf(fp, "  entry_count:   %" PRIu64 "\n", count);
  fprintf(fp, "  next_seq:      %" PRIu64 "\n", next_seq);
  fprintf(fp, "  tail_sha256:   %s\n", tail[0] ? tail : "(empty)");
}
