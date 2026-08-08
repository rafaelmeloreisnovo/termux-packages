#ifndef PKG_SCANNER_H
#define PKG_SCANNER_H

/*
 * REAL: Package Inventory Scanner
 * Status: REAL — scans actual filesystem, no simulation
 *
 * Scans packages/, root-packages/, x11-packages/, disabled-packages/
 * and produces authoritative inventory of every real build.sh.
 *
 * Item #1 from consolidation list: real inventory of packages.
 */

#include <stdint.h>
#include <stdio.h>

#define PKG_NAME_MAX      128
#define PKG_PATH_MAX      512
#define PKG_REPO_MAX      64

typedef enum {
  PKG_REPO_MAIN = 0,        /* packages/ */
  PKG_REPO_ROOT = 1,        /* root-packages/ */
  PKG_REPO_X11 = 2,         /* x11-packages/ */
  PKG_REPO_DISABLED = 3,    /* disabled-packages/ */
  PKG_REPO_UNKNOWN = 255
} pkg_repo_t;

typedef struct {
  char name[PKG_NAME_MAX];        /* directory name OR subpackage name */
  char path[PKG_PATH_MAX];        /* full path to build.sh or *.subpackage.sh */
  char parent[PKG_NAME_MAX];      /* if subpackage: parent pkg name; else same as name */
  pkg_repo_t repo;                /* which repo this package lives in */
  uint64_t build_sh_size;         /* real file size in bytes */
  uint8_t has_build_sh;           /* 1 if build.sh exists and is readable */
  uint8_t is_subpackage;          /* 1 if entry is a *.subpackage.sh */
} pkg_inventory_entry_t;

typedef struct {
  pkg_inventory_entry_t *entries;
  uint32_t count;
  uint32_t capacity;
  uint32_t total_scanned;         /* total directories inspected */
  uint32_t total_with_build_sh;   /* directories that have valid build.sh */
  uint32_t total_missing;         /* dirs without build.sh (TOKEN_VAZIO) */
  uint32_t total_subpackages;     /* *.subpackage.sh entries appended */
} pkg_inventory_t;

/* Initialize inventory (allocate initial capacity). */
int pkg_inventory_init(pkg_inventory_t *inv, uint32_t initial_capacity);

/* Scan a single repo directory (e.g. "packages"). Appends to inventory. */
int pkg_inventory_scan_repo(pkg_inventory_t *inv, const char *repo_dir,
                            pkg_repo_t repo_type);

/* Scan all known repos in the given base directory. */
int pkg_inventory_scan_all(pkg_inventory_t *inv, const char *base_dir);

/* Print inventory as JSON to `out`. */
void pkg_inventory_write_json(FILE *out, const pkg_inventory_t *inv);

/* Print human summary to `out`. */
void pkg_inventory_report(FILE *out, const pkg_inventory_t *inv);

/* Find entry by name (linear scan). Returns NULL if not found. */
const pkg_inventory_entry_t *
pkg_inventory_find(const pkg_inventory_t *inv, const char *name);

void pkg_inventory_free(pkg_inventory_t *inv);

#endif /* PKG_SCANNER_H */
