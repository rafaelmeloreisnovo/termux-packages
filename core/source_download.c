#include "source_download.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define SOURCE_CACHE_DIR ".cache/sources"
#define SOURCE_TEMP_DIR ".tmp"
#define SOURCE_MAX_SIZE (1024 * 1024 * 1024)  /* 1 GB limit */
#define SHA256_HEX_LEN 64
#define SHA256_BLOCK_SIZE 4096

/* Convert 8 x 32-bit SHA256 words to hex string */
static int termux_sha256_to_hex(const uint32_t *sha256_8, char *out_hex, size_t out_len) {
  if (!sha256_8 || !out_hex || out_len < (SHA256_HEX_LEN + 1)) {
    return -1;
  }
  int pos = 0;
  for (int i = 0; i < 8; i++) {
    uint32_t word = sha256_8[i];
    for (int j = 0; j < 4; j++) {
      uint8_t byte = (word >> (24 - 8 * j)) & 0xFF;
      pos += snprintf(out_hex + pos, out_len - pos, "%02x", byte);
    }
  }
  return 0;
}

/* Compute SHA-256 of file and compare with expected hash */
static int termux_verify_sha256(const char *file_path, const uint32_t *expected_sha256_8) {
  if (!file_path || !expected_sha256_8) {
    return -1;
  }

  /* Use sha256sum command if available */
  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", file_path);

  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    fprintf(stderr, "[verify-sha256] Failed to run sha256sum\n");
    return -1;
  }

  char actual_hex[SHA256_HEX_LEN + 1];
  if (!fgets(actual_hex, sizeof(actual_hex), pipe)) {
    fprintf(stderr, "[verify-sha256] No output from sha256sum\n");
    pclose(pipe);
    return -1;
  }
  pclose(pipe);

  /* Extract first SHA256_HEX_LEN characters (the hash) */
  actual_hex[SHA256_HEX_LEN] = '\0';

  /* Convert expected hash to hex */
  char expected_hex[SHA256_HEX_LEN + 1];
  if (termux_sha256_to_hex(expected_sha256_8, expected_hex, sizeof(expected_hex)) != 0) {
    fprintf(stderr, "[verify-sha256] Failed to convert expected hash\n");
    return -1;
  }

  /* Compare */
  if (strcmp(actual_hex, expected_hex) != 0) {
    fprintf(stderr, "[verify-sha256] Hash mismatch: expected=%s actual=%s\n",
            expected_hex, actual_hex);
    return -1;  /* SOURCE_HASH_MISMATCH */
  }

  return 0;  /* Hash verified */
}

/* Construct cache path from SHA256 hash */
static int termux_cache_path(char *out_path, size_t out_len, const uint32_t *sha256_8) {
  if (!out_path || out_len < 512 || !sha256_8) {
    return -1;
  }

  char hex[SHA256_HEX_LEN + 1];
  if (termux_sha256_to_hex(sha256_8, hex, sizeof(hex)) != 0) {
    return -1;
  }

  snprintf(out_path, out_len, "%s/%s.tar.gz", SOURCE_CACHE_DIR, hex);
  return 0;
}

/* Extract tarball to destination directory */
static int termux_extract_tarball(const char *tar_path, const char *dest_dir) {
  if (!tar_path || !dest_dir) {
    return -1;
  }

  /* Create destination directory if needed */
  if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "[extract] Failed to create dest_dir: %s\n", dest_dir);
    return -1;
  }

  /* Extract with tar */
  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>&1", tar_path, dest_dir);

  int ret = system(cmd);
  if (ret != 0) {
    fprintf(stderr, "[extract] tar failed with exit code %d\n", ret);
    return -1;
  }

  fprintf(stderr, "[extract] Extracted to %s\n", dest_dir);
  return 0;
}

/* Download file from URL with size limit */
static int termux_download_source(const char *url, const char *dest_path, size_t max_size) {
  if (!url || !dest_path) {
    return -1;
  }

  /* Create temp directory if needed */
  if (mkdir(SOURCE_TEMP_DIR, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "[download] Failed to create temp dir\n");
    return -1;
  }

  /* Use curl with size limit and timeout */
  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "curl -fsSL --max-filesize %zu --connect-timeout 10 --max-time 300 "
           "    -o '%s' '%s' 2>&1",
           max_size, dest_path, url);

  int ret = system(cmd);
  if (ret != 0) {
    fprintf(stderr, "[download] curl failed with exit code %d\n", ret);
    unlink(dest_path);  /* Clean up partial file */
    return -1;
  }

  /* Verify file was created and is readable */
  struct stat st;
  if (stat(dest_path, &st) != 0) {
    fprintf(stderr, "[download] Downloaded file not found\n");
    return -1;
  }

  fprintf(stderr, "[download] Downloaded %lld bytes\n", (long long)st.st_size);
  return 0;
}

/* Main source acquisition primitive used by the build state machine. */
int termux_acquire_source(struct termux_build_context *ctx) {
  if (!ctx) return -1;

  const char *url = termux_get_string(ctx->pkg.source_url_offset);

  if (!url || url[0] == '\0') {
    fprintf(stderr, "[get-source] SOURCE_URL_MISSING\n");
    return 76;  /* SOURCE_URL_MISSING */
  }

  fprintf(stderr, "[get-source] mode=MANIFEST_ACQUIRE url=%s\n", url);

  /* Construct cache path from SHA256 */
  char cache_path[512];
  if (termux_cache_path(cache_path, sizeof(cache_path), ctx->pkg.sha256) != 0) {
    fprintf(stderr, "[get-source] SOURCE_CACHE_CORRUPT\n");
    return 73;
  }

  /* Check if cache directory exists, create if needed */
  if (mkdir(".cache", 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "[get-source] Failed to create .cache directory\n");
    return 73;  /* SOURCE_CACHE_CORRUPT */
  }
  if (mkdir(".cache/sources", 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "[get-source] Failed to create .cache/sources directory\n");
    return 73;  /* SOURCE_CACHE_CORRUPT */
  }

  /* Check if already cached */
  struct stat cache_stat;
  if (stat(cache_path, &cache_stat) == 0 && S_ISREG(cache_stat.st_mode)) {
    fprintf(stderr, "[get-source] cache_hit\n");
    fprintf(stderr, "[get-source] cache_path=%s\n", cache_path);

    /* Verify cached file still matches hash (corruption check) */
    if (termux_verify_sha256(cache_path, ctx->pkg.sha256) == 0) {
      /* Extract to source_dir */
      if (termux_extract_tarball(cache_path, ctx->source_dir) != 0) {
        fprintf(stderr, "[get-source] SOURCE_EXTRACT_FAILED\n");
        return 80;
      }
      return 0;
    }

    /* Cache corrupted, remove it */
    fprintf(stderr, "[get-source] cache corrupted, removing\n");
    unlink(cache_path);
  }

  /* Download to temp file */
  char temp_path[512];
  snprintf(temp_path, sizeof(temp_path), "%s/source-%d.tar.gz", SOURCE_TEMP_DIR, getpid());

  fprintf(stderr, "[get-source] downloading...\n");
  int ret = termux_download_source(url, temp_path, SOURCE_MAX_SIZE);
  if (ret != 0) {
    fprintf(stderr, "[get-source] SOURCE_NETWORK_FAILURE\n");
    return 74;
  }

  /* Verify SHA-256 */
  fprintf(stderr, "[get-source] verifying SHA-256\n");
  if (termux_verify_sha256(temp_path, ctx->pkg.sha256) != 0) {
    unlink(temp_path);
    fprintf(stderr, "[get-source] SOURCE_HASH_MISMATCH (terminal)\n");
    return 78;  /* SOURCE_HASH_MISMATCH - TERMINAL, NO RETRY */
  }

  /* Atomic cache promotion */
  fprintf(stderr, "[get-source] promoting to cache\n");
  if (rename(temp_path, cache_path) != 0) {
    fprintf(stderr, "[get-source] SOURCE_CACHE_CORRUPT (rename failed)\n");
    unlink(temp_path);
    return 79;
  }

  /* Extract to source_dir */
  fprintf(stderr, "[get-source] extracting...\n");
  if (termux_extract_tarball(cache_path, ctx->source_dir) != 0) {
    fprintf(stderr, "[get-source] SOURCE_EXTRACT_FAILED\n");
    return 80;
  }

  fprintf(stderr, "[get-source] success\n");
  return 0;
}
