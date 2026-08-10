/*
 * REAL: runtime architecture capability probe.
 * Status: OBSERVED — every field is read from the running system.
 *
 * Contract: arch_capability/1.0.0
 *
 * Observes what real_arch's nominal table cannot promise:
 *   - real page size          (sysconf(_SC_PAGE_SIZE))
 *   - real L1 D-cache line    (/sys/devices/system/cpu/cpu0/cache/index0/
 *                                coherency_line_size, or sysconf fallback)
 *   - real SIMD flags         (parsed from /proc/cpuinfo, Linux)
 *   - real uname identity     (uname(2))
 *
 * Then compares against the nominal table in real_arch and reports each
 * field as MATCH or MISMATCH. Mismatches are NOT errors — they are
 * observed truth that supersedes nominal. The nominal table is a
 * catalog default; this binary produces the authoritative observation.
 *
 * Emits both the JSON and a signed receipt (arch_capability/1.0.0).
 *
 * Usage: arch-probe <output_json_path>
 * Exit codes: 0 success, 2 I/O/usage error.
 */

#include "real_arch.h"
#include "real_provenance.h"
#include "real_receipt.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define ARCH_CAPABILITY_SCHEMA "arch_capability/1.0.0"

static uint64_t now_unix_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
}

/* Read L1 D-cache line size in bytes. Returns 0 if unknown; sets *src to
 * the actual source used ("/sys", "sysconf", "unavailable"). */
static uint32_t observe_cache_line(const char **src) {
  /* Try /sys/devices first — most authoritative on Linux */
  FILE *f = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
  if (f) {
    uint32_t v = 0;
    if (fscanf(f, "%u", &v) == 1 && v > 0) {
      fclose(f);
      *src = "/sys/devices/.../coherency_line_size";
      return v;
    }
    fclose(f);
  }
  /* Fall back to sysconf */
  long lc = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  if (lc > 0) {
    *src = "sysconf(_SC_LEVEL1_DCACHE_LINESIZE)";
    return (uint32_t)lc;
  }
  *src = "PROBE_UNAVAILABLE_ON_THIS_OS";
  return 0;
}

/* Read cpuinfo and set SIMD bits by known flag names. Linux only.
 * Sets *src to actual source or PROBE_UNAVAILABLE_ON_THIS_OS. */
static uint32_t observe_simd(const char **src) {
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (!f) {
    *src = "PROBE_UNAVAILABLE_ON_THIS_OS";
    return 0;
  }
  *src = "/proc/cpuinfo";
  uint32_t flags = 0;
  char line[8192];
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "flags", 5) != 0 && strncmp(line, "Features", 8) != 0)
      continue;
    if (strstr(line, " mmx"))    flags |= REAL_SIMD_MMX;
    if (strstr(line, " sse2"))   flags |= REAL_SIMD_SSE2;
    if (strstr(line, " avx2"))   flags |= REAL_SIMD_AVX2;
    if (strstr(line, " avx512"))  flags |= REAL_SIMD_AVX512;
    if (strstr(line, " neon") || strstr(line, " asimd"))
                                 flags |= REAL_SIMD_NEON;
    if (strstr(line, " sve"))    flags |= REAL_SIMD_SVE;
    if (strstr(line, " altivec")) flags |= REAL_SIMD_ALTIVEC;
    break;
  }
  fclose(f);
  return flags;
}

/* Write SIMD flags as JSON array */
static void write_simd_array(FILE *f, uint32_t simd) {
  const struct { uint32_t m; const char *n; } t[] = {
      {REAL_SIMD_MMX, "mmx"},   {REAL_SIMD_SSE2, "sse2"},
      {REAL_SIMD_AVX2, "avx2"}, {REAL_SIMD_AVX512, "avx512"},
      {REAL_SIMD_NEON, "neon"}, {REAL_SIMD_SVE, "sve"},
      {REAL_SIMD_RVV, "rvv"},   {REAL_SIMD_ALTIVEC, "altivec"},
  };
  int first = 1;
  fputc('[', f);
  for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
    if (simd & t[i].m) {
      if (!first) fputc(',', f);
      fprintf(f, "\"%s\"", t[i].n);
      first = 0;
    }
  }
  fputc(']', f);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <output_json_path>\n", argv[0]);
    return 2;
  }
  const char *out_path = argv[1];

  /* Receipt first — captures who/when. */
  real_receipt_t receipt;
  int have_receipt = (real_receipt_begin(&receipt, "arch_probe",
                                          argv[0]) == 0);

  /* Runtime identity */
  struct utsname u;
  if (uname(&u) != 0) {
    fprintf(stderr, "BLOCKED: uname failed\n");
    return 2;
  }
  real_arch_t rt = real_arch_detect_runtime();
  real_arch_t ct = real_arch_compile_time();
  const real_arch_props_t *nominal = real_arch_props(rt);

  /* Observed values with explicit source tracking (no silent 0 leaks) */
  long pg = sysconf(_SC_PAGE_SIZE);
  uint32_t observed_page = pg > 0 ? (uint32_t)pg : 0;
  const char *page_src  = (pg > 0) ? "sysconf(_SC_PAGE_SIZE)"
                                   : "PROBE_UNAVAILABLE_ON_THIS_OS";
  const char *cache_src = "PROBE_UNAVAILABLE_ON_THIS_OS";
  const char *simd_src  = "PROBE_UNAVAILABLE_ON_THIS_OS";
  uint32_t observed_cache = observe_cache_line(&cache_src);
  uint32_t observed_simd  = observe_simd(&simd_src);

  /* Compare against nominal */
  int page_match  = nominal ? (observed_page == nominal->page_size) : 0;
  int cache_match = nominal ? (observed_cache == nominal->cache_line) : 0;
  int simd_match  = nominal ? (observed_simd  == nominal->simd)       : 0;
  /* SIMD "match" is strict here — subset is not full match. */
  int simd_superset =
      nominal ? ((observed_simd & nominal->simd) == nominal->simd) : 0;

  FILE *f = fopen(out_path, "w");
  if (!f) {
    fprintf(stderr, "BLOCKED: cannot open %s: %s\n", out_path, strerror(errno));
    return 2;
  }
  fprintf(f, "{\n");
  fprintf(f, "  \"schema\": \"" ARCH_CAPABILITY_SCHEMA "\",\n");
  fprintf(f, "  \"status\": \"OBSERVED\",\n");
  fprintf(f, "  \"generated_unix_ms\": %" PRIu64 ",\n", now_unix_ms());
  fprintf(f, "  \"identity\": {\n");
  fprintf(f, "    \"compile_time\": \"%s\",\n", real_arch_name(ct));
  fprintf(f, "    \"runtime\":      \"%s\",\n", real_arch_name(rt));
  fprintf(f, "    \"uname_sysname\": \"%s\",\n", u.sysname);
  fprintf(f, "    \"uname_release\": \"%s\",\n", u.release);
  fprintf(f, "    \"uname_machine\": \"%s\"\n",  u.machine);
  fprintf(f, "  },\n");
  fprintf(f, "  \"observed\": {\n");
  fprintf(f, "    \"page_size\":  %u,\n", observed_page);
  fprintf(f, "    \"cache_line\": %u,\n", observed_cache);
  fprintf(f, "    \"simd\": ");
  write_simd_array(f, observed_simd);
  fprintf(f, "\n  },\n");
  fprintf(f, "  \"sources\": {\n");
  fprintf(f, "    \"page_size\":  \"%s\",\n", page_src);
  fprintf(f, "    \"cache_line\": \"%s\",\n", cache_src);
  fprintf(f, "    \"simd\":       \"%s\"\n",  simd_src);
  fprintf(f, "  },\n");
  if (nominal) {
    fprintf(f, "  \"nominal\": {\n");
    fprintf(f, "    \"page_size\":  %u,\n", nominal->page_size);
    fprintf(f, "    \"cache_line\": %u,\n", nominal->cache_line);
    fprintf(f, "    \"simd\": ");
    write_simd_array(f, nominal->simd);
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"comparison\": {\n");
    fprintf(f, "    \"page_size\":  \"%s\",\n",
            page_match ? "MATCH" : "OBSERVED_MISMATCH");
    fprintf(f, "    \"cache_line\": \"%s\",\n",
            cache_match ? "MATCH" : "OBSERVED_MISMATCH");
    fprintf(f, "    \"simd\":       \"%s\",\n",
            simd_match ? "MATCH"
                       : (simd_superset ? "OBSERVED_SUPERSET"
                                        : "OBSERVED_MISMATCH"));
    fprintf(f, "    \"authority\":  \"observed_supersedes_nominal\"\n");
    fprintf(f, "  }\n");
  } else {
    fprintf(f, "  \"nominal\": null,\n");
    fprintf(f, "  \"comparison\": {\"authority\":\"observed_only\"}\n");
  }
  fprintf(f, "}\n");
  /* B11 fix: capture ferror BEFORE fclose so ENOSPC/EIO during
   * fprintf is surfaced. */
  int stream_err = ferror(f);
  if (fclose(f) != 0 || stream_err) {
    fprintf(stderr, "BLOCKED: write/close failed for %s (stream_err=%d)\n",
            out_path, stream_err);
    unlink(out_path);
    return 2;
  }

  fprintf(stdout,
          "arch_capability written to %s runtime=%s page=%u/nominal=%u "
          "cache=%u/nominal=%u simd=0x%02x/nominal=0x%02x\n",
          out_path, real_arch_name(rt), observed_page,
          nominal ? nominal->page_size : 0,
          observed_cache,
          nominal ? nominal->cache_line : 0,
          observed_simd,
          nominal ? nominal->simd : 0);

  if (have_receipt) {
    if (real_receipt_add_output(&receipt, out_path) == 0 &&
        real_receipt_seal(&receipt, 0) == 0) {
      char rcpt_path[512];
      snprintf(rcpt_path, sizeof(rcpt_path), "%s.receipt", out_path);
      if (real_receipt_write(&receipt, rcpt_path) == 0) {
        fprintf(stdout, "OBSERVED receipt sealed: %s (sha=%.16s...)\n",
                rcpt_path, receipt.content_sha256_hex);
      }
    }
  }
  return 0;
}
