#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

/* Manifest types (from manifest_loader.h) */
struct termux_manifest_entry {
  uint32_t version;
  uint32_t sha256[8];
  size_t source_url_offset;
  size_t patches_offset;
  size_t configure_args_offset;
  size_t custom_steps_offset;
};

struct termux_build_context {
  struct {
    uint32_t sha256[8];
    size_t source_url_offset;
  } pkg;
  char source_dir[512];
  char build_dir[512];
};

/* Stub: termux_get_string simulates manifest string lookup */
static char *string_pool[32];
static const char *termux_get_string(size_t offset) {
  if (offset < 32) return string_pool[offset];
  return NULL;
}

/* Compute SHA-256 of file contents */
static int compute_file_sha256(const char *filepath, unsigned char *digest) {
  FILE *fp = fopen(filepath, "rb");
  if (!fp) {
    perror("fopen");
    return -1;
  }

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    fclose(fp);
    return -1;
  }

  if (!EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
    EVP_MD_CTX_free(mdctx);
    fclose(fp);
    return -1;
  }

  unsigned char buffer[4096];
  size_t bytes;
  while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
    if (!EVP_DigestUpdate(mdctx, buffer, bytes)) {
      EVP_MD_CTX_free(mdctx);
      fclose(fp);
      return -1;
    }
  }

  unsigned int digest_len = 0;
  if (!EVP_DigestFinal_ex(mdctx, digest, &digest_len)) {
    EVP_MD_CTX_free(mdctx);
    fclose(fp);
    return -1;
  }

  EVP_MD_CTX_free(mdctx);
  fclose(fp);
  return 0;
}

/* Convert SHA-256 digest to hex string */
static void sha256_to_hex(unsigned char *digest, char *hexout) {
  for (int i = 0; i < 32; i++) {
    sprintf(hexout + (i * 2), "%02x", digest[i]);
  }
  hexout[64] = '\0';
}

/* Apply single patch file with hash binding */
static int apply_patch(const char *patch_path, const char *source_dir,
                       char *patch_receipt) {
  /* Verify patch file exists and is readable */
  if (access(patch_path, R_OK) != 0) {
    fprintf(stderr, "PATCH_MISSING: %s\n", patch_path);
    return 77; /* PATCH_NOT_FOUND */
  }

  /* Compute hash of patch file (pre-application) */
  unsigned char patch_digest[32];
  if (compute_file_sha256(patch_path, patch_digest) != 0) {
    fprintf(stderr, "PATCH_HASH_FAIL: %s\n", patch_path);
    return 74; /* PATCH_DIGEST_ERROR */
  }

  char patch_hash[65];
  sha256_to_hex(patch_digest, patch_hash);

  /* Apply patch using patch(1) command */
  char cmd[2048];
  snprintf(cmd, sizeof(cmd), "cd '%s' && patch -p1 --strict < '%s' 2>&1",
           source_dir, patch_path);

  int ret = system(cmd);
  if (ret != 0) {
    fprintf(stderr, "PATCH_APPLY_FAIL: %s (exit %d)\n", patch_path, ret);
    return 75; /* PATCH_APPLICATION_FAILED */
  }

  /* Log receipt */
  char basename_buf[256];
  strncpy(basename_buf, patch_path, sizeof(basename_buf) - 1);
  const char *patch_name = basename(basename_buf);

  snprintf(patch_receipt, 1024,
           "  patch: %s\n  hash: %s\n  status: APPLIED\n",
           patch_name, patch_hash);

  return 0;
}

/* Main entry point: apply all patches for a package */
int termux_apply_patches(struct termux_build_context *ctx) {
  if (!ctx) {
    fprintf(stderr, "ERROR: NULL context\n");
    return 79; /* INVALID_CONTEXT */
  }

  if (ctx->source_dir[0] == '\0') {
    fprintf(stderr, "ERROR: source_dir not set\n");
    return 73; /* SOURCE_DIR_MISSING */
  }

  /* Verify source directory exists */
  struct stat st;
  if (stat(ctx->source_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "ERROR: source_dir not accessible: %s\n", ctx->source_dir);
    return 73; /* SOURCE_DIR_MISSING */
  }

  /* Get patches string from manifest */
  const char *patches_str = termux_get_string(ctx->pkg.source_url_offset + 1);
  if (!patches_str || patches_str[0] == '\0') {
    /* No patches specified - this is OK */
    fprintf(stdout, "No patches to apply\n");
    return 0;
  }

  fprintf(stdout, "=== PATCH_APPLY receipt ===\n");
  fprintf(stdout, "source_dir: %s\n", ctx->source_dir);
  fprintf(stdout, "patches_list:\n");

  /* Parse patches list (comma-separated filenames) */
  char patches_copy[4096];
  strncpy(patches_copy, patches_str, sizeof(patches_copy) - 1);

  int patch_count = 0;
  int total_patches = 0;

  /* Count patches first */
  char *ptr = patches_copy;
  while (*ptr) {
    if (*ptr == ',') total_patches++;
    ptr++;
  }
  total_patches++;

  /* Parse and apply each patch */
  char patch_list[4096] = "";
  char *saveptr = NULL;
  char *patch_entry = strtok_r(patches_copy, ",", &saveptr);

  while (patch_entry) {
    /* Trim whitespace */
    while (*patch_entry == ' ') patch_entry++;
    char *end = patch_entry + strlen(patch_entry) - 1;
    while (end > patch_entry && *end == ' ') *end-- = '\0';

    /* Construct full patch path */
    char patch_path[1024];
    snprintf(patch_path, sizeof(patch_path), "%s/%s", ctx->source_dir, patch_entry);

    /* Apply patch */
    char receipt[1024] = "";
    int ret = apply_patch(patch_path, ctx->source_dir, receipt);

    if (ret != 0) {
      fprintf(stderr, "PATCH FAILED: %s\n", patch_entry);
      return ret;
    }

    strcat(patch_list, receipt);
    patch_count++;
    patch_entry = strtok_r(NULL, ",", &saveptr);
  }

  fprintf(stdout, "%s", patch_list);
  fprintf(stdout, "total_patches_applied: %d\n", patch_count);
  fprintf(stdout, "status: PASS\n");

  return 0;
}
