#ifndef REAL_PROVENANCE_H
#define REAL_PROVENANCE_H

/*
 * REAL: provenance record — who/what/when/where produced this data.
 * Status: REAL — every field comes from a REAL source at compile or run time.
 *
 * Contract:
 *   git_commit            — full 40-char SHA of the tree that built the binary.
 *                           MUST be non-empty. Injected via -DREAL_GIT_COMMIT="…".
 *   build_timestamp_utc   — ISO-8601 timestamp of binary compile time.
 *                           Injected via -DREAL_BUILD_TIMESTAMP="…".
 *   toolchain_id          — compiler name+version at compile time
 *                           (__VERSION__ macro).
 *   cflags_fingerprint    — SHA-truncated form of build flags used.
 *                           Injected via -DREAL_CFLAGS_FP="…".
 *   producer_name         — argv[0] at run time; identifies which binary.
 *   run_timestamp_unix_ms — CLOCK_REALTIME at run time.
 *   host_uname            — uname(2) at run time.
 *   schema_version        — semantic version of the OUTPUT schema.
 *
 * All fields MUST be present in the output. Any missing field is
 * treated as TOKEN_VAZIO and MUST fail governance validation.
 */

#include "real_attrs.h"
#include <stdint.h>
#include <stdio.h>

#ifndef REAL_GIT_COMMIT
#  define REAL_GIT_COMMIT "TOKEN_VAZIO_no_git_commit_defined_at_build"
#endif

#ifndef REAL_BUILD_TIMESTAMP
#  define REAL_BUILD_TIMESTAMP "TOKEN_VAZIO_no_build_timestamp_defined"
#endif

#ifndef REAL_CFLAGS_FP
#  define REAL_CFLAGS_FP "TOKEN_VAZIO_no_cflags_fingerprint_defined"
#endif

typedef struct {
  const char *git_commit;
  const char *build_timestamp_utc;
  const char *cflags_fingerprint;
  char toolchain_id[128];       /* e.g. "gcc 13.3.0" */
  char producer_name[64];       /* from argv[0] basename */
  uint64_t run_timestamp_unix_ms;
  char host_uname[192];         /* "Linux 6.x.y x86_64" */
  const char *schema_version;   /* e.g. "1.0.0" */
} real_provenance_t;

/* Populate provenance record from all real sources.
 * schema_version MUST be a string literal owned by the caller.
 * Returns 0 on success, -1 on any real-source failure (uname, gettime). */
REAL_WARN_UNUSED REAL_NONNULL_ALL
int real_provenance_capture(real_provenance_t *out,
                            const char *argv0,
                            const char *schema_version);

/* Emit the provenance block as JSON.
 * Writes exactly the fields listed in the contract. */
REAL_COLD REAL_NONNULL_ALL
void real_provenance_write_json(FILE *out, const real_provenance_t *p);

/* Human-readable dump (for logs / --report). */
REAL_COLD REAL_NONNULL_ALL
void real_provenance_report(FILE *out, const real_provenance_t *p);

#endif /* REAL_PROVENANCE_H */
