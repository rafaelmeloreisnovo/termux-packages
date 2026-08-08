#ifndef PKG_SCANNER_H
#define PKG_SCANNER_H

/*
 * Package Inventory Scanner — evidence-first.
 * Status: OBSERVED_LIMITED.
 *
 * The scanner reads the actual filesystem, but filesystem I/O alone is not an
 * authoritative completeness proof. Coverage of the four known repository
 * roots and every collection failure is recorded explicitly. No absent root,
 * path overflow, allocation failure or subpackage scan failure may disappear
 * silently.
 *
 * Compatibility note:
 *   The concurrent `build_sh/repo_type/roots_scanned` API is preserved as C11
 *   union aliases of `path/repo/roots_present`; there is one storage location
 *   and therefore no split state.
 */

#include "real_attrs.h"
#include <stdint.h>
#include <stdio.h>

#define PKG_NAME_MAX      128
#define PKG_PATH_MAX      512
#define PKG_REPO_MAX      64
#define PKG_REPO_EXPECTED_ROOTS 4

typedef enum {
  PKG_REPO_MAIN = 0,
  PKG_REPO_ROOT = 1,
  PKG_REPO_X11 = 2,
  PKG_REPO_DISABLED = 3,
  PKG_REPO_UNKNOWN = 255
} pkg_repo_t;

typedef struct {
  char name[PKG_NAME_MAX];
  union {
    char path[PKG_PATH_MAX];
    char build_sh[PKG_PATH_MAX]; /* compatibility alias */
  };
  char parent[PKG_NAME_MAX];
  union {
    pkg_repo_t repo;
    pkg_repo_t repo_type; /* compatibility alias */
  };
  uint64_t build_sh_size;
  uint8_t has_build_sh;
  uint8_t is_subpackage;
} pkg_inventory_entry_t;

typedef struct {
  pkg_inventory_entry_t *entries;
  uint32_t count;
  uint32_t capacity;

  /* Content counters. */
  uint32_t total_scanned;
  uint32_t total_with_build_sh;
  uint32_t total_missing;
  uint32_t total_subpackages;
  uint32_t non_directory_entries;
  uint32_t non_regular_subpackages;

  /* Coverage ledger for known repository roots. */
  uint32_t roots_expected;
  union {
    uint32_t roots_present;
    uint32_t roots_scanned; /* compatibility alias */
  };
  uint32_t roots_absent;
  uint32_t roots_failed;

  /* Collection failures. Any non-zero error counter blocks completeness. */
  uint32_t path_errors;
  uint32_t io_errors;
  uint32_t allocation_errors;
  uint32_t subpackage_scan_failures;
} pkg_inventory_t;

REAL_WARN_UNUSED REAL_NONNULL(1)
int pkg_inventory_init(pkg_inventory_t *inv, uint32_t initial_capacity);

REAL_HOT REAL_WARN_UNUSED REAL_NONNULL(1, 2)
int pkg_inventory_scan_repo(pkg_inventory_t *inv, const char *repo_dir,
                            pkg_repo_t repo_type);

/* Diagnostic scan: absent known roots are recorded, not silently discarded.
 * Returns -1 on collection failure, but an absent root alone can still return 0
 * so the caller can inspect a partial inventory. */
REAL_HOT REAL_WARN_UNUSED REAL_NONNULL_ALL
int pkg_inventory_scan_all(pkg_inventory_t *inv, const char *base_dir);

/* Compatibility strict scan: succeeds only for a complete four-root census. */
REAL_HOT REAL_WARN_UNUSED REAL_NONNULL_ALL
int pkg_inventory_scan(pkg_inventory_t *inv, const char *repo_root);

/* True only when all expected roots were observed and no collection errors
 * occurred. This says nothing about Bash semantic completeness. */
REAL_PURE REAL_NONNULL(1)
int pkg_inventory_is_complete(const pkg_inventory_t *inv);

REAL_COLD REAL_NONNULL_ALL
void pkg_inventory_write_json(FILE *out, const pkg_inventory_t *inv);

/* Compatibility stdout JSON printer. */
REAL_COLD REAL_NONNULL(1)
void pkg_inventory_print_json(const pkg_inventory_t *inv);

REAL_COLD REAL_NONNULL_ALL
void pkg_inventory_report(FILE *out, const pkg_inventory_t *inv);

REAL_PURE REAL_NONNULL_ALL
const pkg_inventory_entry_t *
pkg_inventory_find(const pkg_inventory_t *inv, const char *name);

void pkg_inventory_free(pkg_inventory_t *inv);
void pkg_inventory_destroy(pkg_inventory_t *inv); /* compatibility alias */

#endif /* PKG_SCANNER_H */
