#include "real_provenance.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>

/* Compile-time toolchain identifier */
#ifdef __clang__
#  define REAL_CC_NAME "clang"
#elif defined(__GNUC__)
#  define REAL_CC_NAME "gcc"
#else
#  define REAL_CC_NAME "cc"
#endif

static void cp(char *dst, size_t cap, const char *src) {
  if (cap == 0) return;
  size_t i = 0;
  while (i + 1 < cap && src && src[i]) { dst[i] = src[i]; i++; }
  dst[i] = '\0';
}

/* Extract basename portion of a path (last slash+1). */
static const char *basename_of(const char *p) {
  const char *last = p;
  for (const char *q = p; *q; q++) if (*q == '/') last = q + 1;
  return last;
}

int real_provenance_capture(real_provenance_t *out,
                            const char *argv0,
                            const char *schema_version) {
  /* NONNULL_ALL contract enforced by compiler */
  memset(out, 0, sizeof(*out));

  out->git_commit           = REAL_GIT_COMMIT;
  out->build_timestamp_utc  = REAL_BUILD_TIMESTAMP;
  out->cflags_fingerprint   = REAL_CFLAGS_FP;
  out->schema_version       = schema_version;

  /* Toolchain — check for truncation; mark honestly if it happens */
  {
    const char *ver =
#ifdef __VERSION__
        __VERSION__
#else
        "unknown"
#endif
        ;
    int n = snprintf(out->toolchain_id, sizeof(out->toolchain_id),
                     "%s %s", REAL_CC_NAME, ver);
    if (n < 0 || (size_t)n >= sizeof(out->toolchain_id)) {
      /* Truncation is a real event — surface via TOKEN_VAZIO so the
       * contract validator refuses to promote the artifact. */
      cp(out->toolchain_id, sizeof(out->toolchain_id),
         "TOKEN_VAZIO_toolchain_id_truncated");
    }
  }

  /* Producer basename */
  cp(out->producer_name, sizeof(out->producer_name), basename_of(argv0));

  /* Run timestamp */
  {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return -1;
    out->run_timestamp_unix_ms =
        (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
  }

  /* Uname — same truncation policy */
  {
    struct utsname u;
    if (uname(&u) != 0) return -1;
    int n = snprintf(out->host_uname, sizeof(out->host_uname),
                     "%s %s %s", u.sysname, u.release, u.machine);
    if (n < 0 || (size_t)n >= sizeof(out->host_uname)) {
      cp(out->host_uname, sizeof(out->host_uname),
         "TOKEN_VAZIO_host_uname_truncated");
    }
  }

  return 0;
}

/* D4 fix: JSON-escape helper for provenance writer. Same policy as
 * arch_probe's and pkg_scanner's escape (C17/C10) — handles `"`, `\`,
 * `\n`, `\r`, `\t`, control chars via `\uXXXX`. Symmetric with
 * real_contract.c's extract_str decode (D3). Prevents provenance
 * fields with future non-ASCII content (custom-kernel uname, exotic
 * toolchain reports containing `"`) from silently corrupting the
 * receipt/metrics JSON. Today's producers write only ASCII into these
 * fields, so this fix is prospective. */
static void prov_json_esc(FILE *out, const char *s) {
  fputc('"', out);
  if (!s) { fputc('"', out); return; }
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    switch (*p) {
      case '"':  fputs("\\\"", out); break;
      case '\\': fputs("\\\\", out); break;
      case '\n': fputs("\\n", out);  break;
      case '\r': fputs("\\r", out);  break;
      case '\t': fputs("\\t", out);  break;
      default:
        if (*p < 0x20) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
  }
  fputc('"', out);
}

void real_provenance_write_json(FILE *out, const real_provenance_t *p) {
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "  \"provenance\": {\n");
  fputs("    \"schema_version\": ", out);      prov_json_esc(out, p->schema_version);      fputs(",\n", out);
  fputs("    \"git_commit\": ", out);          prov_json_esc(out, p->git_commit);          fputs(",\n", out);
  fputs("    \"build_timestamp_utc\": ", out); prov_json_esc(out, p->build_timestamp_utc); fputs(",\n", out);
  fputs("    \"cflags_fingerprint\": ", out);  prov_json_esc(out, p->cflags_fingerprint);  fputs(",\n", out);
  fputs("    \"toolchain_id\": ", out);        prov_json_esc(out, p->toolchain_id);        fputs(",\n", out);
  fputs("    \"producer_name\": ", out);       prov_json_esc(out, p->producer_name);       fputs(",\n", out);
  fprintf(out, "    \"run_timestamp_unix_ms\": %" PRIu64 ",\n",
          p->run_timestamp_unix_ms);
  fputs("    \"host_uname\": ", out);          prov_json_esc(out, p->host_uname);          fputs("\n", out);
  fprintf(out, "  }");
}

void real_provenance_report(FILE *out, const real_provenance_t *p) {
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "=== REAL Provenance ===\n");
  fprintf(out, "  schema_version:       %s\n", p->schema_version);
  fprintf(out, "  git_commit:           %s\n", p->git_commit);
  fprintf(out, "  build_timestamp_utc:  %s\n", p->build_timestamp_utc);
  fprintf(out, "  cflags_fingerprint:   %s\n", p->cflags_fingerprint);
  fprintf(out, "  toolchain_id:         %s\n", p->toolchain_id);
  fprintf(out, "  producer_name:        %s\n", p->producer_name);
  fprintf(out, "  run_timestamp_unix_ms: %" PRIu64 "\n",
          p->run_timestamp_unix_ms);
  fprintf(out, "  host_uname:           %s\n", p->host_uname);
}
