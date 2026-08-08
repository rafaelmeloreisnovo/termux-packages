#include "pkg_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * REAL: Faithful build.sh Parser
 * No shell execution. Line-by-line lex of TERMUX_PKG_<KEY>=<VAL> assignments.
 * ============================================================================ */

static void copy_str(char *dst, size_t cap, const char *src) {
  if (!dst || cap == 0) return;
  if (!src) { dst[0] = '\0'; return; }
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

/* Strip surrounding quotes ("..." or '...') and trailing whitespace. */
static void unquote_inplace(char *s) {
  if (!s) return;
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                     s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[--len] = '\0';
  }
  if (len >= 2) {
    if ((s[0] == '"' && s[len - 1] == '"') ||
        (s[0] == '\'' && s[len - 1] == '\'')) {
      memmove(s, s + 1, len - 2);
      s[len - 2] = '\0';
    }
  }
}

/* Detect unresolved ${...} or $VAR in value. */
static int has_unresolved_expansion(const char *s) {
  for (const char *p = s; *p; p++) {
    if (*p == '$' && (p[1] == '{' || (isalpha((unsigned char)p[1]) || p[1] == '_'))) {
      return 1;
    }
  }
  return 0;
}

/* Skip lines inside shell functions. Track brace depth once we enter one. */
static int line_starts_function(const char *line) {
  /* matches: `NAME() {` optionally with whitespace */
  const char *p = line;
  while (*p == ' ' || *p == '\t') p++;
  const char *start = p;
  while (isalnum((unsigned char)*p) || *p == '_') p++;
  if (p == start) return 0;
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '(') return 0;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  if (*p != ')') return 0;
  return 1;
}

/* Extract raw comma-separated list into destination if key matches. */
static void maybe_capture(const char *key, const char *value,
                          pkg_parser_result_t *r) {
  if (strcmp(key, "TERMUX_PKG_VERSION") == 0) {
    copy_str(r->version, sizeof(r->version), value);
  } else if (strcmp(key, "TERMUX_PKG_HOMEPAGE") == 0) {
    copy_str(r->homepage, sizeof(r->homepage), value);
  } else if (strcmp(key, "TERMUX_PKG_DESCRIPTION") == 0) {
    copy_str(r->description, sizeof(r->description), value);
  } else if (strcmp(key, "TERMUX_PKG_LICENSE") == 0) {
    copy_str(r->license, sizeof(r->license), value);
  } else if (strcmp(key, "TERMUX_PKG_MAINTAINER") == 0) {
    copy_str(r->maintainer, sizeof(r->maintainer), value);
  } else if (strcmp(key, "TERMUX_PKG_SRCURL") == 0) {
    copy_str(r->srcurl, sizeof(r->srcurl), value);
    r->has_srcurl = 1;
  } else if (strcmp(key, "TERMUX_PKG_SHA256") == 0) {
    copy_str(r->sha256, sizeof(r->sha256), value);
    r->has_sha256 = 1;
  } else if (strcmp(key, "TERMUX_PKG_DEPENDS") == 0) {
    copy_str(r->depends_raw, sizeof(r->depends_raw), value);
    r->has_depends = 1;
  } else if (strcmp(key, "TERMUX_PKG_BUILD_DEPENDS") == 0) {
    copy_str(r->build_depends_raw, sizeof(r->build_depends_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_RECOMMENDS") == 0) {
    copy_str(r->recommends_raw, sizeof(r->recommends_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_SUGGESTS") == 0) {
    copy_str(r->suggests_raw, sizeof(r->suggests_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_CONFLICTS") == 0) {
    copy_str(r->conflicts_raw, sizeof(r->conflicts_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_BREAKS") == 0) {
    copy_str(r->breaks_raw, sizeof(r->breaks_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_REPLACES") == 0) {
    copy_str(r->replaces_raw, sizeof(r->replaces_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_PROVIDES") == 0) {
    copy_str(r->provides_raw, sizeof(r->provides_raw), value);
  } else if (strcmp(key, "TERMUX_PKG_ANTI_BUILD_DEPENDS") == 0) {
    copy_str(r->anti_build_depends_raw, sizeof(r->anti_build_depends_raw),
             value);
  } else if (strcmp(key, "TERMUX_PKG_ESSENTIAL") == 0) {
    copy_str(r->essential, sizeof(r->essential), value);
  } else if (strcmp(key, "TERMUX_PKG_PLATFORM_INDEPENDENT") == 0) {
    copy_str(r->platform_independent, sizeof(r->platform_independent), value);
  } else if (strcmp(key, "TERMUX_PKG_EXCLUDED_ARCHES") == 0) {
    copy_str(r->excluded_arches, sizeof(r->excluded_arches), value);
  } else if (strcmp(key, "TERMUX_PKG_METAPACKAGE") == 0) {
    copy_str(r->metapackage, sizeof(r->metapackage), value);
  }
}

int pkg_parser_parse_file(const char *build_sh_path, pkg_parser_result_t *out) {
  if (!build_sh_path || !out) return -1;
  memset(out, 0, sizeof(*out));

  /* Derive name from parent directory (real behavior). */
  const char *slash = strrchr(build_sh_path, '/');
  if (slash) {
    const char *end = slash;
    const char *start = slash;
    while (start > build_sh_path && *(start - 1) != '/') start--;
    size_t n = (size_t)(end - start);
    if (n >= sizeof(out->name)) n = sizeof(out->name) - 1;
    memcpy(out->name, start, n);
    out->name[n] = '\0';
  }

  FILE *f = fopen(build_sh_path, "r");
  if (!f) return -1;

  char line[PKG_PARSER_MAX_VAL];
  int in_function = 0;
  int brace_depth = 0;

  while (fgets(line, sizeof(line), f)) {
    out->lines_read++;

    /* Trim leading whitespace for detection but keep original for content. */
    const char *lead = line;
    while (*lead == ' ' || *lead == '\t') lead++;

    /* Skip comments (unless inside quoted strings — rare in headers). */
    if (*lead == '#' || *lead == '\n' || *lead == '\r' || *lead == '\0') {
      continue;
    }

    /* Track function bodies (we ignore assignments inside them). */
    if (!in_function && line_starts_function(lead)) {
      in_function = 1;
      brace_depth = 0;
      for (const char *p = lead; *p; p++) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
      }
      if (brace_depth <= 0) in_function = 0;
      continue;
    }
    if (in_function) {
      for (const char *p = lead; *p; p++) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
      }
      if (brace_depth <= 0) in_function = 0;
      continue;
    }

    /* Match TERMUX_PKG_<key>=<value>. */
    if (strncmp(lead, "TERMUX_PKG_", 11) != 0) continue;
    const char *eq = strchr(lead, '=');
    if (!eq) continue;

    size_t klen = (size_t)(eq - lead);
    if (klen >= PKG_PARSER_MAX_KEY) klen = PKG_PARSER_MAX_KEY - 1;

    char key[PKG_PARSER_MAX_KEY];
    memcpy(key, lead, klen);
    key[klen] = '\0';

    /* Value: everything after '=' */
    char value[PKG_PARSER_MAX_VAL];
    copy_str(value, sizeof(value), eq + 1);

    /* Handle line continuation: strip trailing '\n' + '\\' and read next. */
    size_t vlen = strlen(value);
    while (vlen >= 2 && value[vlen - 1] == '\n' && value[vlen - 2] == '\\') {
      value[vlen - 2] = '\0';
      char cont[PKG_PARSER_MAX_VAL];
      if (!fgets(cont, sizeof(cont), f)) break;
      out->lines_read++;
      /* strip trailing newline of continuation */
      size_t clen = strlen(cont);
      if (clen > 0 && cont[clen - 1] == '\n') cont[--clen] = '\0';
      /* concat with bounds */
      size_t remaining = sizeof(value) - strlen(value) - 1;
      strncat(value, cont, remaining);
      vlen = strlen(value);
    }

    unquote_inplace(value);

    /* Store in generic vars array. */
    if (out->var_count < PKG_PARSER_MAX_VARS) {
      pkg_parser_var_t *v = &out->vars[out->var_count++];
      copy_str(v->key, sizeof(v->key), key);
      copy_str(v->value, sizeof(v->value), value);
      v->unresolved_expansion = (uint8_t)has_unresolved_expansion(value);
      if (v->unresolved_expansion) out->vars_with_expansions++;
    }

    /* Populate typed fields. */
    maybe_capture(key, value, out);
  }

  fclose(f);
  out->parse_ok = 1;
  return 0;
}

const pkg_parser_var_t *
pkg_parser_get(const pkg_parser_result_t *r, const char *key) {
  if (!r || !key) return NULL;
  for (uint32_t i = 0; i < r->var_count; i++) {
    if (strcmp(r->vars[i].key, key) == 0) return &r->vars[i];
  }
  return NULL;
}

static void json_esc(FILE *out, const char *s) {
  if (!s) { fputs("\"\"", out); return; }
  fputc('"', out);
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    switch (*p) {
      case '"':  fputs("\\\"", out); break;
      case '\\': fputs("\\\\", out); break;
      case '\n': fputs("\\n", out); break;
      case '\r': fputs("\\r", out); break;
      case '\t': fputs("\\t", out); break;
      default:
        if (*p < 0x20) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
  }
  fputc('"', out);
}

void pkg_parser_write_json(FILE *out, const pkg_parser_result_t *r) {
  if (!out || !r) return;
  fputs("{\n", out);
  fputs("  \"schema\": \"pkg_parser_v1\",\n", out);
  fputs("  \"status\": \"REAL\",\n", out);
  fputs("  \"name\": ", out);           json_esc(out, r->name);           fputs(",\n", out);
  fputs("  \"version\": ", out);        json_esc(out, r->version);        fputs(",\n", out);
  fputs("  \"homepage\": ", out);       json_esc(out, r->homepage);       fputs(",\n", out);
  fputs("  \"description\": ", out);    json_esc(out, r->description);    fputs(",\n", out);
  fputs("  \"license\": ", out);        json_esc(out, r->license);        fputs(",\n", out);
  fputs("  \"maintainer\": ", out);     json_esc(out, r->maintainer);     fputs(",\n", out);
  fputs("  \"srcurl\": ", out);         json_esc(out, r->srcurl);         fputs(",\n", out);
  fputs("  \"sha256\": ", out);         json_esc(out, r->sha256);         fputs(",\n", out);
  fputs("  \"depends_raw\": ", out);    json_esc(out, r->depends_raw);    fputs(",\n", out);
  fputs("  \"build_depends_raw\": ", out); json_esc(out, r->build_depends_raw); fputs(",\n", out);
  fputs("  \"recommends_raw\": ", out); json_esc(out, r->recommends_raw); fputs(",\n", out);
  fputs("  \"suggests_raw\": ", out);   json_esc(out, r->suggests_raw);   fputs(",\n", out);
  fputs("  \"conflicts_raw\": ", out);  json_esc(out, r->conflicts_raw);  fputs(",\n", out);
  fputs("  \"replaces_raw\": ", out);   json_esc(out, r->replaces_raw);   fputs(",\n", out);
  fputs("  \"provides_raw\": ", out);   json_esc(out, r->provides_raw);   fputs(",\n", out);
  fprintf(out, "  \"lines_read\": %u,\n", r->lines_read);
  fprintf(out, "  \"var_count\": %u,\n", r->var_count);
  fprintf(out, "  \"vars_with_expansions\": %u,\n", r->vars_with_expansions);
  fprintf(out, "  \"parse_ok\": %s\n", r->parse_ok ? "true" : "false");
  fputs("}\n", out);
}

void pkg_parser_report(FILE *out, const pkg_parser_result_t *r) {
  if (!out || !r) return;
  fprintf(out, "=== REAL Parser: %s ===\n", r->name);
  fprintf(out, "  Version:      %s\n", r->version);
  fprintf(out, "  License:      %s\n", r->license);
  fprintf(out, "  SRC URL:      %s%s\n", r->srcurl,
          r->has_srcurl ? "" : "  [TOKEN_VAZIO: no SRCURL]");
  fprintf(out, "  SHA256:       %s%s\n", r->sha256,
          r->has_sha256 ? "" : "  [TOKEN_VAZIO: no SHA256]");
  fprintf(out, "  Depends:      %s\n", r->depends_raw);
  fprintf(out, "  Build deps:   %s\n", r->build_depends_raw);
  fprintf(out, "  Vars parsed:  %u (%u with unresolved ${...} expansions)\n",
          r->var_count, r->vars_with_expansions);
  fprintf(out, "  Lines read:   %u\n", r->lines_read);
}
