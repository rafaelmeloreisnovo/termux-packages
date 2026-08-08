#include "../source_download.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_source_url[2048];
static int g_url_enabled = 1;

const char *termux_get_string(uint32_t offset) {
  (void)offset;
  return g_url_enabled ? g_source_url : NULL;
}

static int fail(const char *message) {
  fprintf(stderr, "SOURCE_ACQUISITION_RUNTIME=FAIL reason=%s\n", message);
  return 1;
}

static int file_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int sha256_words(const char *path, uint32_t out[8]) {
  char cmd[4096];
  if (snprintf(cmd, sizeof(cmd), "sha256sum '%s'", path) >= (int)sizeof(cmd)) return -1;
  FILE *pipe = popen(cmd, "r");
  if (!pipe) return -1;
  char hex[65];
  if (!fgets(hex, sizeof(hex), pipe)) {
    pclose(pipe);
    return -1;
  }
  if (pclose(pipe) != 0) return -1;
  hex[64] = '\0';
  for (int i = 0; i < 8; i++) {
    char word[9];
    memcpy(word, hex + (i * 8), 8);
    word[8] = '\0';
    char *end = NULL;
    unsigned long value = strtoul(word, &end, 16);
    if (!end || *end != '\0') return -1;
    out[i] = (uint32_t)value;
  }
  return 0;
}

int main(void) {
  char root[] = "/tmp/rafcodephi-source-runtime-XXXXXX";
  if (!mkdtemp(root)) return fail("mkdtemp");
  if (chdir(root) != 0) return fail("chdir-root");

  if (mkdir("payload", 0755) != 0) return fail("mkdir-payload");
  FILE *marker = fopen("payload/marker.txt", "w");
  if (!marker) return fail("create-marker");
  fputs("RAFCODEPHI_SOURCE_FIXTURE_V1\n", marker);
  if (fclose(marker) != 0) return fail("close-marker");

  if (system("tar -czf source.tar.gz payload") != 0) return fail("create-tarball");

  char archive[2048];
  if (!realpath("source.tar.gz", archive)) return fail("realpath-archive");
  if (snprintf(g_source_url, sizeof(g_source_url), "file://%s", archive) >= (int)sizeof(g_source_url)) {
    return fail("url-too-long");
  }

  struct termux_build_context ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.pkg.source_url_offset = 1;
  if (sha256_words(archive, ctx.pkg.sha256) != 0) return fail("sha256-fixture");

  if (snprintf(ctx.source_dir, sizeof(ctx.source_dir), "%s/out-source-1", root) >= (int)sizeof(ctx.source_dir)) {
    return fail("source-dir-1-too-long");
  }
  int rc = termux_acquire_source(&ctx);
  if (rc != 0) return fail("fresh-acquisition-returned-nonzero");

  char extracted[2048];
  if (snprintf(extracted, sizeof(extracted), "%s/payload/marker.txt", ctx.source_dir) >= (int)sizeof(extracted)) {
    return fail("marker-path-too-long");
  }
  if (!file_exists(extracted)) return fail("fresh-acquisition-did-not-extract-marker");

  if (snprintf(ctx.source_dir, sizeof(ctx.source_dir), "%s/out-source-2", root) >= (int)sizeof(ctx.source_dir)) {
    return fail("source-dir-2-too-long");
  }
  rc = termux_acquire_source(&ctx);
  if (rc != 0) return fail("cache-hit-returned-nonzero");
  if (snprintf(extracted, sizeof(extracted), "%s/payload/marker.txt", ctx.source_dir) >= (int)sizeof(extracted)) {
    return fail("marker-path-2-too-long");
  }
  if (!file_exists(extracted)) return fail("cache-hit-did-not-extract-marker");

  uint32_t original = ctx.pkg.sha256[0];
  ctx.pkg.sha256[0] ^= 0x01000000u;
  if (snprintf(ctx.source_dir, sizeof(ctx.source_dir), "%s/out-source-badhash", root) >= (int)sizeof(ctx.source_dir)) {
    return fail("source-dir-badhash-too-long");
  }
  rc = termux_acquire_source(&ctx);
  if (rc != 78) return fail("hash-mismatch-did-not-fail-closed-with-78");
  ctx.pkg.sha256[0] = original;

  g_url_enabled = 0;
  if (snprintf(ctx.source_dir, sizeof(ctx.source_dir), "%s/out-source-no-url", root) >= (int)sizeof(ctx.source_dir)) {
    return fail("source-dir-no-url-too-long");
  }
  rc = termux_acquire_source(&ctx);
  if (rc != 76) return fail("missing-url-did-not-fail-closed-with-76");

  printf("SOURCE_ACQUISITION_LOCAL_RUNTIME=PASS fresh=PASS cache_hit=PASS hash_mismatch=PASS missing_url=PASS remote_network=NOT_MEASURED claim_allowed=false\n");
  return 0;
}
