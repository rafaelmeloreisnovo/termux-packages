#ifndef PKG_PARSER_H
#define PKG_PARSER_H

/*
 * REAL: build.sh Parser
 * Status: REAL — reads actual build.sh files and extracts TERMUX_PKG_* vars.
 *
 * Item #2 from consolidation list: faithful parser of build.sh.
 * Also supports subpackage discovery (foo.subpackage.sh) — item #4/#12.
 *
 * Semantics:
 *  - Parses top-level `TERMUX_PKG_<KEY>=<value>` assignments.
 *  - Handles single-quoted, double-quoted, and unquoted values.
 *  - Handles line continuations with trailing backslash.
 *  - Does NOT execute shell (no expansion of $(...), $VAR, arithmetic).
 *  - Records unresolved variable references verbatim, marked as such.
 *  - Ignores content inside shell functions (termux_step_*() { ... }).
 */

#include "real_attrs.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define PKG_PARSER_MAX_KEY   64
#define PKG_PARSER_MAX_VAL   1024
#define PKG_PARSER_MAX_VARS  128
#define PKG_PARSER_MAX_DEPS  256

typedef struct {
  char key[PKG_PARSER_MAX_KEY];
  char value[PKG_PARSER_MAX_VAL];
  uint8_t unresolved_expansion;  /* 1 if value contains ${...} we did not expand */
} pkg_parser_var_t;

typedef struct {
  /* Basic identity */
  char name[64];
  char version[64];
  char homepage[256];
  char description[512];
  char license[128];
  char maintainer[128];

  /* Source */
  char srcurl[512];
  char sha256[65];      /* 64 hex chars + NUL */

  /* Dependency lists (comma-separated, split later by dep extractor) */
  char depends_raw[PKG_PARSER_MAX_VAL];
  char build_depends_raw[PKG_PARSER_MAX_VAL];
  char recommends_raw[PKG_PARSER_MAX_VAL];
  char suggests_raw[PKG_PARSER_MAX_VAL];
  char conflicts_raw[PKG_PARSER_MAX_VAL];
  char breaks_raw[PKG_PARSER_MAX_VAL];
  char replaces_raw[PKG_PARSER_MAX_VAL];
  char provides_raw[PKG_PARSER_MAX_VAL];
  char anti_build_depends_raw[PKG_PARSER_MAX_VAL];

  /* Build tuning flags (bool-ish strings from build.sh) */
  char essential[16];              /* true/false or empty */
  char platform_independent[16];
  char excluded_arches[128];
  char metapackage[16];

  /* All parsed variables (for full fidelity) */
  pkg_parser_var_t vars[PKG_PARSER_MAX_VARS];
  uint32_t var_count;

  /* Diagnostics */
  uint32_t lines_read;
  uint32_t vars_with_expansions;   /* count with unresolved ${...} */
  uint8_t  parse_ok;               /* 1 if file was fully read */
  uint8_t  has_srcurl;
  uint8_t  has_sha256;
  uint8_t  has_depends;
} pkg_parser_result_t;

/* Parse a single build.sh into `out`. Zeros out `out` first. */
REAL_HOT REAL_WARN_UNUSED REAL_NONNULL_ALL
int pkg_parser_parse_file(const char *build_sh_path, pkg_parser_result_t *out);

/* Look up a variable by key; returns NULL if not found. */
REAL_PURE REAL_NONNULL_ALL
const pkg_parser_var_t *
pkg_parser_get(const pkg_parser_result_t *r, const char *key);

/* Emit parsed result as JSON to `out`. */
REAL_COLD REAL_NONNULL_ALL
void pkg_parser_write_json(FILE *out, const pkg_parser_result_t *r);

/* Human summary. */
REAL_COLD REAL_NONNULL_ALL
void pkg_parser_report(FILE *out, const pkg_parser_result_t *r);

#endif /* PKG_PARSER_H */
