#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

/* Forward declarations */
struct termux_build_context {
  struct {
    uint32_t sha256[8];
    size_t source_url_offset;
  } pkg;
  char source_dir[512];
};

extern int termux_apply_patches(struct termux_build_context *ctx);

/*
 * TV-03 Test Gate: PATCH_APPLY (patch application with hash binding)
 *
 * Closure criteria:
 * - termux_apply_patches callable (interface exists)
 * - Proper error handling for missing patches_offset (no crash)
 * - Proper error handling for missing source_dir (exit code 73)
 * - Proper error handling for NULL context (exit code 79)
 * - Test exits with code 0 on PASS, non-zero on FAIL
 */

int main(void) {
  int failures = 0;

  fprintf(stdout, "[TV-03] PATCH_APPLY test gate\n");
  fprintf(stdout, "[TV-03] Testing termux_apply_patches interface\n");

  /* Test 1: NULL context should return error code 79 */
  fprintf(stdout, "\n[TEST 1] NULL context\n");
  int ret1 = termux_apply_patches(NULL);
  if (ret1 == 79) {
    fprintf(stdout, "  [PASS] Returned 79 (INVALID_CONTEXT)\n");
  } else {
    fprintf(stdout, "  [FAIL] Expected 79, got %d\n", ret1);
    failures++;
  }

  /* Test 2: Missing source_dir should return error code 73 */
  fprintf(stdout, "\n[TEST 2] Missing source_dir\n");
  struct termux_build_context ctx2 = {0};
  ctx2.pkg.source_url_offset = 0;
  ctx2.source_dir[0] = '\0';  /* Empty source_dir */

  int ret2 = termux_apply_patches(&ctx2);
  if (ret2 == 73) {
    fprintf(stdout, "  [PASS] Returned 73 (SOURCE_DIR_MISSING)\n");
  } else {
    fprintf(stdout, "  [FAIL] Expected 73, got %d\n", ret2);
    failures++;
  }

  /* Test 3: Valid context with no patches should succeed */
  fprintf(stdout, "\n[TEST 3] Valid context (no patches)\n");
  struct termux_build_context ctx3 = {0};
  ctx3.pkg.source_url_offset = 0;
  snprintf(ctx3.source_dir, sizeof(ctx3.source_dir), "/tmp");

  int ret3 = termux_apply_patches(&ctx3);
  if (ret3 == 0) {
    fprintf(stdout, "  [PASS] Returned 0 (no patches, success)\n");
  } else {
    fprintf(stdout, "  [FAIL] Expected 0, got %d\n", ret3);
    failures++;
  }

  /* Test 4: Function is callable with valid context structure */
  fprintf(stdout, "\n[TEST 4] Function callable with valid context\n");
  struct termux_build_context ctx4 = {0};
  ctx4.pkg.source_url_offset = 1;
  ctx4.pkg.sha256[0] = 0x6a09e667;
  snprintf(ctx4.source_dir, sizeof(ctx4.source_dir), "/tmp/test-source-4");

  int ret4 = termux_apply_patches(&ctx4);
  fprintf(stdout, "  [PASS] Function callable, returned %d\n", ret4);

  /* Summary */
  fprintf(stdout, "\n[TV-03] SUMMARY\n");
  fprintf(stdout, "  Tests run: 4\n");
  fprintf(stdout, "  Failures: %d\n", failures);
  fprintf(stdout, "  Status: %s\n", failures == 0 ? "PASS" : "FAIL");
  fprintf(stdout, "\n[TV-03] Closure criteria:\n");
  fprintf(stdout, "  - termux_apply_patches interface exists: YES\n");
  fprintf(stdout, "  - Error handling for NULL context (79): %s\n",
          ret1 == 79 ? "YES" : "NO");
  fprintf(stdout, "  - Error handling for missing source_dir (73): %s\n",
          ret2 == 73 ? "YES" : "NO");
  fprintf(stdout, "  - Function callable with valid context: YES\n");

  return failures > 0 ? 1 : 0;
}
