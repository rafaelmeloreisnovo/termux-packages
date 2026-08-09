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

void real_provenance_write_json(FILE *out, const real_provenance_t *p) {
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "  \"provenance\": {\n");
  fprintf(out, "    \"schema_version\": \"%s\",\n", p->schema_version);
  fprintf(out, "    \"git_commit\": \"%s\",\n", p->git_commit);
  fprintf(out, "    \"build_timestamp_utc\": \"%s\",\n",
          p->build_timestamp_utc);
  fprintf(out, "    \"cflags_fingerprint\": \"%s\",\n",
          p->cflags_fingerprint);
  fprintf(out, "    \"toolchain_id\": \"%s\",\n", p->toolchain_id);
  fprintf(out, "    \"producer_name\": \"%s\",\n", p->producer_name);
  fprintf(out, "    \"run_timestamp_unix_ms\": %" PRIu64 ",\n",
          p->run_timestamp_unix_ms);
  fprintf(out, "    \"host_uname\": \"%s\"\n", p->host_uname);
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
