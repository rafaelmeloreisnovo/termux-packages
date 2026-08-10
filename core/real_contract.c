#include "real_contract.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * REAL: minimal JSON field extractor + contract validator.
 * Designed to avoid pulling a full JSON parser; keeps binary small.
 * Extracts flat keys "key": value and one nested "provenance": {…} block.
 * ============================================================================ */

static void add_violation(real_contract_report_t *r, const char *field,
                          const char *reason) {
  if (r->count >= sizeof(r->items) / sizeof(r->items[0])) return;
  real_contract_violation_t *v = &r->items[r->count++];
  strncpy(v->field, field, sizeof(v->field) - 1);
  v->field[sizeof(v->field) - 1] = '\0';
  strncpy(v->reason, reason, sizeof(v->reason) - 1);
  v->reason[sizeof(v->reason) - 1] = '\0';
}

/* Find "key" pattern; return pointer to first char after the following colon,
 * or NULL if not found. Only matches at top level or inside `scope` if scope
 * is non-NULL (searches within the scope substring). */
static const char *find_key(const char *hay, const char *key) {
  char pattern[80];
  int n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if (n <= 0 || (size_t)n >= sizeof(pattern)) return NULL;
  const char *p = strstr(hay, pattern);
  if (!p) return NULL;
  p += n;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (*p != ':') return NULL;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

/* Extract quoted string starting at *cursor; fills dst up to cap-1 chars.
 * Returns 1 on success, 0 on failure.
 *
 * D3 fix: properly DECODE JSON escape sequences (\", \\, \/, \n, \r, \t,
 * \b, \f) instead of silently dropping the escaped char. Before this
 * fix, an input of `"foo\"bar"` would produce `"foobar"` in dst — both
 * the `\` and the escaped char were consumed but nothing was written.
 * This meant any producer that emitted an escape sequence would produce
 * a canonical form the verifier couldn't reconstruct, causing SHA
 * mismatch on legitimate receipts. Today no REAL producer writes escape
 * sequences into pkg_metrics fields (all are ASCII: git hash, ISO-8601
 * timestamp, uname), so the bug was latent — but D4/D5 add json_esc
 * to the writers, and that only round-trips correctly if this decode
 * is symmetric. `\uXXXX` is intentionally NOT handled (no producer
 * emits it and the parser is only used with our own output). */
static int extract_str(const char *cursor, char *dst, size_t cap) {
  if (*cursor != '"') return 0;
  cursor++;
  size_t i = 0;
  while (*cursor && *cursor != '"' && i + 1 < cap) {
    if (*cursor == '\\' && cursor[1]) {
      char decoded;
      switch (cursor[1]) {
        case '"':  decoded = '"';  break;
        case '\\': decoded = '\\'; break;
        case '/':  decoded = '/';  break;
        case 'n':  decoded = '\n'; break;
        case 'r':  decoded = '\r'; break;
        case 't':  decoded = '\t'; break;
        case 'b':  decoded = '\b'; break;
        case 'f':  decoded = '\f'; break;
        default:
          /* Unknown escape: keep the raw char after the backslash to
           * preserve some information. Producer/verifier still stay in
           * sync because both see the raw bytes. */
          decoded = cursor[1];
          break;
      }
      dst[i++] = decoded;
      cursor += 2;
    } else {
      dst[i++] = *cursor++;
    }
  }
  dst[i] = '\0';
  return *cursor == '"';
}

/* Extract a numeric value (uint32/uint64/double as text). Returns 1 on
 * success; caller parses. Writes up to cap-1 chars. */
static int extract_num(const char *cursor, char *dst, size_t cap) {
  size_t i = 0;
  while (*cursor &&
         (isdigit((unsigned char)*cursor) || *cursor == '.' ||
          *cursor == '-' || *cursor == '+' || *cursor == 'e' ||
          *cursor == 'E') &&
         i + 1 < cap) {
    dst[i++] = *cursor++;
  }
  dst[i] = '\0';
  return i > 0;
}

static void require_u32(const char *json, const char *key, uint32_t *out,
                        real_contract_report_t *r) {
  const char *c = find_key(json, key);
  if (!c) { add_violation(r, key, "field missing"); return; }
  char buf[32];
  if (!extract_num(c, buf, sizeof(buf))) {
    add_violation(r, key, "not numeric");
    return;
  }
  errno = 0;
  unsigned long v = strtoul(buf, NULL, 10);
  if (errno || v > (unsigned long)UINT32_MAX) {
    add_violation(r, key, "out of range for uint32");
    return;
  }
  *out = (uint32_t)v;
}

static void require_u64(const char *json, const char *key, uint64_t *out,
                        real_contract_report_t *r) {
  const char *c = find_key(json, key);
  if (!c) { add_violation(r, key, "field missing"); return; }
  char buf[32];
  if (!extract_num(c, buf, sizeof(buf))) {
    add_violation(r, key, "not numeric");
    return;
  }
  errno = 0;
  unsigned long long v = strtoull(buf, NULL, 10);
  if (errno) { add_violation(r, key, "out of range for uint64"); return; }
  *out = (uint64_t)v;
}

static void require_double(const char *json, const char *key, double *out,
                           double lo, double hi,
                           real_contract_report_t *r) {
  const char *c = find_key(json, key);
  if (!c) { add_violation(r, key, "field missing"); return; }
  char buf[64];
  if (!extract_num(c, buf, sizeof(buf))) {
    add_violation(r, key, "not numeric");
    return;
  }
  /* B2 fix: reset+check errno around strtod; also enforce that the
   * parser consumed at least one char (endptr moved). Silent overflow
   * (HUGE_VAL) or empty parse would otherwise be caught only by the
   * range check — and only for bounded fields. */
  errno = 0;
  char *endp = NULL;
  double v = strtod(buf, &endp);
  if (errno != 0 || endp == buf) {
    add_violation(r, key, "unparseable or out-of-range double");
    return;
  }
  if (v < lo || v > hi) {
    char msg[128];
    snprintf(msg, sizeof(msg), "%.6f outside [%.3f, %.3f]", v, lo, hi);
    add_violation(r, key, msg);
    return;
  }
  *out = v;
}

static void require_str(const char *json, const char *key, char *dst,
                        size_t cap, real_contract_report_t *r) {
  const char *c = find_key(json, key);
  if (!c) { add_violation(r, key, "field missing"); return; }
  if (!extract_str(c, dst, cap)) {
    add_violation(r, key, "not a string or overflow");
    return;
  }
  if (dst[0] == '\0') { add_violation(r, key, "empty string"); return; }
  if (strstr(dst, "TOKEN_VAZIO") != NULL) {
    add_violation(r, key, "contains TOKEN_VAZIO placeholder");
  }
}

int real_contract_validate_file(const char *json_path,
                                real_contract_v1_t *out,
                                real_contract_report_t *report) {
  /* NONNULL_ALL contract enforced by compiler */
  memset(out, 0, sizeof(*out));
  memset(report, 0, sizeof(*report));

  FILE *f = fopen(json_path, "r");
  if (!f) {
    add_violation(report, "file", "cannot open json_path");
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 1 << 20) {
    add_violation(report, "file", "empty or > 1MB");
    fclose(f);
    return -1;
  }
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); add_violation(report, "file", "oom"); return -1; }
  size_t nr = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[nr] = '\0';

  /* Top-level status must be "REAL" */
  {
    char status[16] = {0};
    const char *c = find_key(buf, "status");
    if (!c || !extract_str(c, status, sizeof(status))) {
      add_violation(report, "status", "field missing or not string");
    } else if (strcmp(status, "REAL") != 0) {
      char msg[64];
      snprintf(msg, sizeof(msg), "not REAL (got \"%s\")", status);
      add_violation(report, "status", msg);
    }
  }

  /* Required counters */
  require_u32(buf, "node_count",         &out->node_count,       report);
  require_u32(buf, "edge_count",         &out->edge_count,       report);
  require_u32(buf, "depends_edges",      &out->depends_edges,    report);
  require_u32(buf, "build_dep_edges",    &out->build_dep_edges,  report);
  require_u32(buf, "unresolved_count",   &out->unresolved_count, report);
  require_u32(buf, "cycle_count",        &out->cycle_count,      report);
  require_u32(buf, "max_depth",          &out->max_depth,        report);
  require_u32(buf, "topo_ordered",       &out->topo_ordered,     report);

  /* Bounded doubles */
  require_double(buf, "coherence_phi",       &out->coherence_phi,       0.0, 1.0, report);
  require_double(buf, "graph_completeness",  &out->graph_completeness,  0.0, 1.0, report);
  require_double(buf, "graph_acyclicity",    &out->graph_acyclicity,    0.0, 1.0, report);
  require_double(buf, "avg_deps_per_pkg",    &out->avg_deps_per_pkg,    0.0, 10000.0, report);

  /* Latencies */
  require_u64(buf, "inventory_latency_us", &out->inventory_latency_us, report);
  require_u64(buf, "dag_latency_us",       &out->dag_latency_us,       report);
  require_u64(buf, "total_latency_us",     &out->total_latency_us,     report);

  /* Cross-field invariants */
  if (out->edge_count != out->depends_edges + out->build_dep_edges) {
    add_violation(report, "edge_count",
                  "edge_count != depends_edges + build_dep_edges");
  }
  if (out->cycle_count == 0 && out->topo_ordered != out->node_count) {
    add_violation(report, "topo_ordered",
                  "no cycles reported yet topo_ordered != node_count");
  }
  if (out->node_count == 0) {
    add_violation(report, "node_count", "must be > 0");
  }
  /* Coherence must equal completeness × acyclicity (definitional).
   * Small tolerance for floating-point rounding. */
  {
    double expected = out->graph_completeness * out->graph_acyclicity;
    double diff = out->coherence_phi - expected;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
      char msg[128];
      snprintf(msg, sizeof(msg),
               "%.6f != completeness (%.6f) × acyclicity (%.6f) = %.6f",
               out->coherence_phi, out->graph_completeness,
               out->graph_acyclicity, expected);
      add_violation(report, "coherence_phi", msg);
    }
  }
  /* Acyclicity must reflect cycles: if cycle_count > 0, acyclicity < 1. */
  if (out->cycle_count > 0 && out->graph_acyclicity >= 1.0) {
    add_violation(report, "graph_acyclicity",
                  "cycles > 0 but acyclicity is 1.0");
  }
  /* Completeness must reflect unresolved. */
  if (out->edge_count > 0) {
    double expected_c =
        1.0 - ((double)out->unresolved_count / (double)out->edge_count);
    double diff = out->graph_completeness - expected_c;
    if (diff < 0) diff = -diff;
    if (diff > 0.0001) {
      char msg[128];
      snprintf(msg, sizeof(msg),
               "%.6f != 1 - unresolved/edges = %.6f",
               out->graph_completeness, expected_c);
      add_violation(report, "graph_completeness", msg);
    }
  }

  /* Provenance block — every string non-empty and TOKEN_VAZIO-free */
  require_str(buf, "schema_version",  out->provenance_schema,
              sizeof(out->provenance_schema), report);
  require_str(buf, "git_commit",      out->provenance_git_commit,
              sizeof(out->provenance_git_commit), report);
  require_str(buf, "build_timestamp_utc", out->provenance_build_ts,
              sizeof(out->provenance_build_ts), report);
  require_str(buf, "toolchain_id",    out->provenance_toolchain,
              sizeof(out->provenance_toolchain), report);
  require_str(buf, "producer_name",   out->provenance_producer,
              sizeof(out->provenance_producer), report);
  require_str(buf, "host_uname",      out->provenance_host,
              sizeof(out->provenance_host), report);
  require_u64(buf, "run_timestamp_unix_ms", &out->provenance_run_ms, report);

  free(buf);
  return report->count == 0 ? 0 : -1;
}

void real_contract_report_print(FILE *fp, const real_contract_report_t *r) {
  /* NONNULL_ALL contract enforced by compiler */
  if (r->count == 0) {
    fprintf(fp, "✓ contract pkg_metrics/1.0.0 — all invariants satisfied\n");
    return;
  }
  fprintf(fp, "✗ contract pkg_metrics/1.0.0 — %u violation(s):\n", r->count);
  for (uint32_t i = 0; i < r->count; i++) {
    fprintf(fp, "  - %-30s : %s\n", r->items[i].field, r->items[i].reason);
  }
}
