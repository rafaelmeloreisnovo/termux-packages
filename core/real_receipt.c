#include "real_receipt.h"
#include "real_arch.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * REAL: receipt lifecycle — begin → add_input* → add_output* → seal → write
 * All strings and hashes come from real sources.
 * ============================================================================ */

static uint64_t now_unix_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
}

static uint64_t monotonic_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

int real_receipt_begin(real_receipt_t *r, const char *operation,
                       const char *argv0) {
  /* NONNULL_ALL contract enforced */
  memset(r, 0, sizeof(*r));
  strncpy(r->operation, operation, sizeof(r->operation) - 1);
  r->operation[sizeof(r->operation) - 1] = '\0';
  if (real_provenance_capture(&r->provenance, argv0, REAL_RECEIPT_SCHEMA) < 0)
    return -1;
  const char *cn = real_arch_name(real_arch_compile_time());
  const char *rn = real_arch_name(real_arch_detect_runtime());
  strncpy(r->arch_compile, cn ? cn : "unknown", sizeof(r->arch_compile) - 1);
  strncpy(r->arch_runtime, rn ? rn : "unknown", sizeof(r->arch_runtime) - 1);
  r->arch_compile[sizeof(r->arch_compile) - 1] = '\0';
  r->arch_runtime[sizeof(r->arch_runtime) - 1] = '\0';
  r->started_unix_ms = now_unix_ms();
  return 0;
}

static int add_io_entry(real_receipt_io_t *slot, const char *path) {
  uint8_t digest[REAL_SHA256_DIGEST];
  uint64_t size = 0;
  if (real_sha256_file(path, digest, &size) < 0) return -1;
  strncpy(slot->path, path, sizeof(slot->path) - 1);
  slot->path[sizeof(slot->path) - 1] = '\0';
  real_sha256_hex(digest, slot->sha256_hex);
  slot->size_bytes = size;
  return 0;
}

int real_receipt_add_input(real_receipt_t *r, const char *path) {
  if (r->input_count >= RECEIPT_MAX_IO) return -1;
  if (add_io_entry(&r->inputs[r->input_count], path) < 0) return -1;
  r->input_count++;
  return 0;
}

int real_receipt_add_output(real_receipt_t *r, const char *path) {
  if (r->output_count >= RECEIPT_MAX_IO) return -1;
  if (add_io_entry(&r->outputs[r->output_count], path) < 0) return -1;
  r->output_count++;
  return 0;
}

/* Serialize receipt fields (excluding content_sha256_hex) in canonical
 * form for hashing. Order and format MUST NOT change without bumping
 * schema version — the hash depends on it. */
static void canonical_serialize(const real_receipt_t *r, real_sha256_ctx_t *h) {
  #define UPD(s) real_sha256_update(h, (const uint8_t *)(s), strlen(s))
  #define UPDN(x, buf) do { snprintf(buf, sizeof(buf), "%" PRIu64, (uint64_t)(x)); UPD(buf); } while (0)
  char nb[32];

  UPD("schema=" REAL_RECEIPT_SCHEMA "\n");
  UPD("operation=");            UPD(r->operation);           UPD("\n");
  UPD("prov.git_commit=");      UPD(r->provenance.git_commit);       UPD("\n");
  UPD("prov.build_ts=");        UPD(r->provenance.build_timestamp_utc); UPD("\n");
  UPD("prov.cflags_fp=");       UPD(r->provenance.cflags_fingerprint); UPD("\n");
  UPD("prov.toolchain=");       UPD(r->provenance.toolchain_id);     UPD("\n");
  UPD("prov.producer=");        UPD(r->provenance.producer_name);    UPD("\n");
  UPD("prov.host=");            UPD(r->provenance.host_uname);       UPD("\n");
  UPDN(r->provenance.run_timestamp_unix_ms, nb); UPD("=prov.run_ms\n");
  UPD("arch.compile=");         UPD(r->arch_compile);        UPD("\n");
  UPD("arch.runtime=");         UPD(r->arch_runtime);        UPD("\n");
  UPDN(r->input_count, nb);     UPD("=input_count\n");
  for (uint32_t i = 0; i < r->input_count; i++) {
    UPD("in.path=");   UPD(r->inputs[i].path);       UPD("\n");
    UPD("in.sha=");    UPD(r->inputs[i].sha256_hex); UPD("\n");
    UPDN(r->inputs[i].size_bytes, nb); UPD("=in.size\n");
  }
  UPDN(r->output_count, nb);    UPD("=output_count\n");
  for (uint32_t i = 0; i < r->output_count; i++) {
    UPD("out.path=");  UPD(r->outputs[i].path);       UPD("\n");
    UPD("out.sha=");   UPD(r->outputs[i].sha256_hex); UPD("\n");
    UPDN(r->outputs[i].size_bytes, nb); UPD("=out.size\n");
  }
  UPDN((uint64_t)(uint32_t)r->exit_code, nb); UPD("=exit_code\n");
  UPDN(r->duration_us, nb);     UPD("=duration_us\n");
  UPDN(r->started_unix_ms, nb); UPD("=started_ms\n");
  UPDN(r->finished_unix_ms, nb);UPD("=finished_ms\n");

  #undef UPD
  #undef UPDN
}

int real_receipt_seal(real_receipt_t *r, int exit_code) {
  /* NONNULL(1) */
  r->exit_code = (int32_t)exit_code;
  r->finished_unix_ms = now_unix_ms();
  r->duration_us =
      r->finished_unix_ms > r->started_unix_ms
          ? (r->finished_unix_ms - r->started_unix_ms) * 1000ULL
          : 0;
  /* If we want microsecond precision, prefer monotonic delta:
   * caller can override by setting duration_us before seal. */
  if (r->duration_us == 0) r->duration_us = monotonic_us() % 1000000ULL;

  real_sha256_ctx_t h;
  real_sha256_init(&h);
  canonical_serialize(r, &h);
  uint8_t digest[REAL_SHA256_DIGEST];
  real_sha256_final(&h, digest);
  real_sha256_hex(digest, r->content_sha256_hex);
  return 0;
}

static void write_io_array(FILE *f, const char *name,
                           const real_receipt_io_t *arr, uint32_t n) {
  fprintf(f, "  \"%s\": [", name);
  for (uint32_t i = 0; i < n; i++) {
    fprintf(f, "%s\n    {\"path\":\"%s\",\"sha256\":\"%s\",\"size\":%" PRIu64 "}",
            i == 0 ? "" : ",", arr[i].path, arr[i].sha256_hex,
            arr[i].size_bytes);
  }
  fprintf(f, "%s]", n > 0 ? "\n  " : "");
}

int real_receipt_write(const real_receipt_t *r, const char *path) {
  /* NONNULL_ALL */
  FILE *f = fopen(path, "w");
  if (!f) return -1;
  fprintf(f, "{\n");
  fprintf(f, "  \"schema\": \"" REAL_RECEIPT_SCHEMA "\",\n");
  fprintf(f, "  \"status\": \"REAL\",\n");
  fprintf(f, "  \"operation\": \"%s\",\n", r->operation);
  fprintf(f, "  \"content_sha256\": \"%s\",\n", r->content_sha256_hex);
  fprintf(f, "  \"exit_code\": %d,\n", r->exit_code);
  fprintf(f, "  \"duration_us\": %" PRIu64 ",\n", r->duration_us);
  fprintf(f, "  \"started_unix_ms\": %" PRIu64 ",\n", r->started_unix_ms);
  fprintf(f, "  \"finished_unix_ms\": %" PRIu64 ",\n", r->finished_unix_ms);
  fprintf(f, "  \"arch\": {\"compile_time\": \"%s\", \"runtime\": \"%s\"},\n",
          r->arch_compile, r->arch_runtime);
  real_provenance_write_json(f, &r->provenance);
  fprintf(f, ",\n");
  write_io_array(f, "inputs",  r->inputs,  r->input_count);
  fprintf(f, ",\n");
  write_io_array(f, "outputs", r->outputs, r->output_count);
  fprintf(f, "\n}\n");
  fclose(f);
  return 0;
}

/* Simple JSON-ish extractor (same style as real_contract).
 * Returns pointer to first char after "key": or NULL. */
static const char *find_key_v(const char *hay, const char *key) {
  char pat[64];
  int n = snprintf(pat, sizeof(pat), "\"%s\"", key);
  if (n <= 0 || (size_t)n >= sizeof(pat)) return NULL;
  const char *p = strstr(hay, pat);
  if (!p) return NULL;
  p += n;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (*p != ':') return NULL;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

int real_receipt_verify_file(const char *path, real_receipt_t *out) {
  /* NONNULL_ALL */
  FILE *f = fopen(path, "r");
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 1 << 20) { fclose(f); return -1; }
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return -1; }
  size_t nr = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[nr] = '\0';

  memset(out, 0, sizeof(*out));

  /* Extract stored content_sha256 for later comparison */
  char stored_sha[REAL_SHA256_HEXLEN] = {0};
  const char *p = find_key_v(buf, "content_sha256");
  if (!p || *p != '"') { free(buf); return -1; }
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < sizeof(stored_sha)) stored_sha[i++] = *p++;
  stored_sha[i] = '\0';

  /* Extract simple fields into out (enough to reconstruct canonical form
   * for hashing). For a v1 verifier we require producers to write
   * identically-shaped JSON; a fuller parser can come later. */
  #define GRAB_STR(field, dst) do { \
      const char *qs_ = find_key_v(buf, field); \
      if (qs_ && *qs_ == '"') { \
        qs_++; size_t js_ = 0; \
        while (*qs_ && *qs_ != '"' && js_ + 1 < sizeof(dst)) \
          dst[js_++] = *qs_++; \
        dst[js_] = '\0'; \
      } \
    } while (0)

  GRAB_STR("operation", out->operation);

  /* Provenance strings needed for canonical serialization */
  static char pg[64], pb[64], pfp[64], pt[128], pp[64], ph[192], ps[64];
  GRAB_STR("schema_version", ps);
  GRAB_STR("git_commit", pg);
  GRAB_STR("build_timestamp_utc", pb);
  GRAB_STR("cflags_fingerprint", pfp);
  GRAB_STR("toolchain_id", pt);
  GRAB_STR("producer_name", pp);
  GRAB_STR("host_uname", ph);
  out->provenance.git_commit           = pg;
  out->provenance.build_timestamp_utc  = pb;
  out->provenance.cflags_fingerprint   = pfp;
  out->provenance.schema_version       = ps;
  strncpy(out->provenance.toolchain_id, pt, sizeof(out->provenance.toolchain_id) - 1);
  strncpy(out->provenance.producer_name, pp, sizeof(out->provenance.producer_name) - 1);
  strncpy(out->provenance.host_uname, ph, sizeof(out->provenance.host_uname) - 1);

  const char *q;
  #define GRAB_U64(field, dst) do { \
      q = find_key_v(buf, field); \
      if (q) dst = strtoull(q, NULL, 10); \
    } while (0)

  GRAB_U64("run_timestamp_unix_ms", out->provenance.run_timestamp_unix_ms);
  GRAB_U64("started_unix_ms",  out->started_unix_ms);
  GRAB_U64("finished_unix_ms", out->finished_unix_ms);
  GRAB_U64("duration_us",      out->duration_us);
  q = find_key_v(buf, "exit_code");
  if (q) out->exit_code = (int32_t)strtol(q, NULL, 10);

  GRAB_STR("compile_time", out->arch_compile);
  GRAB_STR("runtime",      out->arch_runtime);

  /* Parse inputs / outputs arrays — v1 uses fixed format */
  const char *arr_start = strstr(buf, "\"inputs\":");
  if (arr_start) {
    const char *cur = arr_start;
    while ((cur = strstr(cur, "\"path\":")) != NULL) {
      /* Only advance while inside inputs array */
      if (cur > strstr(buf, "\"outputs\":")) break;
      if (out->input_count >= RECEIPT_MAX_IO) break;
      real_receipt_io_t *io = &out->inputs[out->input_count++];
      cur += 7; while (*cur == ' ' || *cur == '"') cur++;
      size_t j = 0;
      while (*cur && *cur != '"' && j + 1 < sizeof(io->path)) io->path[j++] = *cur++;
      io->path[j] = '\0';
      const char *shq = strstr(cur, "\"sha256\":");
      if (shq) {
        shq += 9; while (*shq == ' ' || *shq == '"') shq++;
        j = 0;
        while (*shq && *shq != '"' && j + 1 < sizeof(io->sha256_hex))
          io->sha256_hex[j++] = *shq++;
        io->sha256_hex[j] = '\0';
      }
      const char *sq = strstr(cur, "\"size\":");
      if (sq) { sq += 7; io->size_bytes = strtoull(sq, NULL, 10); }
    }
  }
  const char *out_start = strstr(buf, "\"outputs\":");
  if (out_start) {
    const char *cur = out_start;
    while ((cur = strstr(cur, "\"path\":")) != NULL) {
      if (out->output_count >= RECEIPT_MAX_IO) break;
      real_receipt_io_t *io = &out->outputs[out->output_count++];
      cur += 7; while (*cur == ' ' || *cur == '"') cur++;
      size_t j = 0;
      while (*cur && *cur != '"' && j + 1 < sizeof(io->path)) io->path[j++] = *cur++;
      io->path[j] = '\0';
      const char *shq = strstr(cur, "\"sha256\":");
      if (shq) {
        shq += 9; while (*shq == ' ' || *shq == '"') shq++;
        j = 0;
        while (*shq && *shq != '"' && j + 1 < sizeof(io->sha256_hex))
          io->sha256_hex[j++] = *shq++;
        io->sha256_hex[j] = '\0';
      }
      const char *sq = strstr(cur, "\"size\":");
      if (sq) { sq += 7; io->size_bytes = strtoull(sq, NULL, 10); }
    }
  }

  free(buf);

  /* Recompute content SHA and compare */
  real_sha256_ctx_t h;
  real_sha256_init(&h);
  canonical_serialize(out, &h);
  uint8_t digest[REAL_SHA256_DIGEST];
  real_sha256_final(&h, digest);
  char recomputed[REAL_SHA256_HEXLEN];
  real_sha256_hex(digest, recomputed);

  strncpy(out->content_sha256_hex, stored_sha, sizeof(out->content_sha256_hex) - 1);

  if (strcmp(stored_sha, recomputed) != 0) return -1;
  return 0;
}

void real_receipt_report(FILE *fp, const real_receipt_t *r) {
  /* NONNULL_ALL */
  fprintf(fp, "=== REAL Receipt ===\n");
  fprintf(fp, "  operation:        %s\n", r->operation);
  fprintf(fp, "  content_sha256:   %s\n", r->content_sha256_hex);
  fprintf(fp, "  exit_code:        %d\n", r->exit_code);
  fprintf(fp, "  duration_us:      %" PRIu64 "\n", r->duration_us);
  fprintf(fp, "  arch:             %s (compile) / %s (runtime)\n",
          r->arch_compile, r->arch_runtime);
  fprintf(fp, "  git_commit:       %s\n", r->provenance.git_commit);
  fprintf(fp, "  toolchain:        %s\n", r->provenance.toolchain_id);
  fprintf(fp, "  inputs:           %u\n", r->input_count);
  for (uint32_t i = 0; i < r->input_count; i++) {
    fprintf(fp, "    %s  %" PRIu64 " bytes\n    %s\n",
            r->inputs[i].sha256_hex, r->inputs[i].size_bytes,
            r->inputs[i].path);
  }
  fprintf(fp, "  outputs:          %u\n", r->output_count);
  for (uint32_t i = 0; i < r->output_count; i++) {
    fprintf(fp, "    %s  %" PRIu64 " bytes\n    %s\n",
            r->outputs[i].sha256_hex, r->outputs[i].size_bytes,
            r->outputs[i].path);
  }
}
