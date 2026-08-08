#include "pkg_scanner.h"
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ============================================================================
 * Package Inventory Scanner — OBSERVED_LIMITED.
 *
 * Reads the filesystem directly and keeps an explicit coverage/error ledger.
 * Collection errors are never converted into silently missing packages.
 * ============================================================================ */

REAL_PURE REAL_COLD
static const char *repo_name(pkg_repo_t r) {
  switch (r) {
    case PKG_REPO_MAIN:     return "packages";
    case PKG_REPO_ROOT:     return "root-packages";
    case PKG_REPO_X11:      return "x11-packages";
    case PKG_REPO_DISABLED: return "disabled-packages";
    case PKG_REPO_UNKNOWN:  return "unknown";
  }
  REAL_UNREACHABLE();
}

int pkg_inventory_init(pkg_inventory_t *inv, uint32_t initial_capacity) {
  if (REAL_UNLIKELY(initial_capacity == 0)) return -1;
  memset(inv, 0, sizeof(*inv));
  inv->roots_expected = PKG_REPO_EXPECTED_ROOTS;
  inv->entries =
      (pkg_inventory_entry_t *)calloc(initial_capacity, sizeof(*inv->entries));
  if (REAL_UNLIKELY(!inv->entries)) {
    inv->allocation_errors = 1;
    return -1;
  }
  inv->capacity = initial_capacity;
  return 0;
}

REAL_COLD REAL_NOINLINE
static int inv_grow(pkg_inventory_t *inv) {
  if (inv->capacity > UINT32_MAX / 2U) {
    inv->allocation_errors++;
    return -1;
  }
  uint32_t new_cap = inv->capacity == 0 ? 64U : inv->capacity * 2U;
  size_t new_cap_size = (size_t)new_cap;
  if (new_cap_size != 0 &&
      sizeof(*inv->entries) > SIZE_MAX / new_cap_size) {
    inv->allocation_errors++;
    return -1;
  }
  size_t allocation_size = new_cap_size * sizeof(*inv->entries);
  pkg_inventory_entry_t *n =
      (pkg_inventory_entry_t *)realloc(inv->entries, allocation_size);
  if (REAL_UNLIKELY(!n)) {
    inv->allocation_errors++;
    return -1;
  }
  memset(n + inv->capacity, 0,
         (size_t)(new_cap - inv->capacity) * sizeof(*inv->entries));
  inv->entries = n;
  inv->capacity = new_cap;
  return 0;
}

REAL_HOT
static int scan_subpackages(pkg_inventory_t *inv, const char *pkg_dir,
                            const char *parent_name, pkg_repo_t repo_type) {
  DIR *sd = opendir(pkg_dir);
  if (!sd) {
    inv->subpackage_scan_failures++;
    inv->io_errors++;
    return -1;
  }

  for (;;) {
    errno = 0;
    struct dirent *se = readdir(sd);
    if (!se) {
      if (errno != 0) {
        inv->io_errors++;
        inv->subpackage_scan_failures++;
        closedir(sd);
        return -1;
      }
      break;
    }
    if (se->d_name[0] == '.') continue;
    size_t len = strlen(se->d_name);
    const char *suffix = ".subpackage.sh";
    size_t suflen = strlen(suffix);
    if (len <= suflen) continue;
    if (strcmp(se->d_name + len - suflen, suffix) != 0) continue;

    size_t nm_len = len - suflen;
    if (nm_len == 0 || nm_len >= PKG_NAME_MAX || strlen(parent_name) >= PKG_NAME_MAX) {
      inv->path_errors++;
      closedir(sd);
      return -1;
    }

    char sub_path[PKG_PATH_MAX];
    int n = snprintf(sub_path, sizeof(sub_path), "%s/%s", pkg_dir, se->d_name);
    if (n <= 0 || (size_t)n >= sizeof(sub_path)) {
      inv->path_errors++;
      closedir(sd);
      return -1;
    }

    struct stat st;
    if (stat(sub_path, &st) != 0) {
      inv->io_errors++;
      inv->subpackage_scan_failures++;
      closedir(sd);
      return -1;
    }
    if (!S_ISREG(st.st_mode)) continue;

    if (inv->count >= inv->capacity && inv_grow(inv) != 0) {
      closedir(sd);
      return -1;
    }
    pkg_inventory_entry_t *entry = &inv->entries[inv->count++];
    memset(entry, 0, sizeof(*entry));
    entry->repo_type = repo_type;
    entry->is_subpackage = 1;
    if (snprintf(entry->name, sizeof(entry->name), "%s:%.*s", parent_name,
                 (int)nm_len, se->d_name) <= 0 ||
        snprintf(entry->build_sh, sizeof(entry->build_sh), "%s", sub_path) <= 0) {
      inv->path_errors++;
      closedir(sd);
      return -1;
    }
  }

  closedir(sd);
  return 0;
}

static int scan_repo_root(pkg_inventory_t *inv, const char *root,
                          pkg_repo_t repo_type) {
  DIR *dir = opendir(root);
  if (!dir) {
    inv->roots_failed++;
    inv->io_errors++;
    return -1;
  }
  inv->roots_scanned++;

  for (;;) {
    errno = 0;
    struct dirent *de = readdir(dir);
    if (!de) {
      if (errno != 0) {
        inv->io_errors++;
        inv->roots_failed++;
        closedir(dir);
        return -1;
      }
      break;
    }
    if (de->d_name[0] == '.') continue;

    char pkg_dir[PKG_PATH_MAX];
    char build_sh[PKG_PATH_MAX];
    int n1 = snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", root, de->d_name);
    int n2 = snprintf(build_sh, sizeof(build_sh), "%s/build.sh", pkg_dir);
    if (n1 <= 0 || (size_t)n1 >= sizeof(pkg_dir) || n2 <= 0 ||
        (size_t)n2 >= sizeof(build_sh)) {
      inv->path_errors++;
      closedir(dir);
      return -1;
    }

    struct stat st_dir;
    if (stat(pkg_dir, &st_dir) != 0) {
      inv->io_errors++;
      closedir(dir);
      return -1;
    }
    if (!S_ISDIR(st_dir.st_mode)) continue;

    struct stat st_build;
    if (stat(build_sh, &st_build) != 0) {
      if (errno == ENOENT) continue;
      inv->io_errors++;
      closedir(dir);
      return -1;
    }
    if (!S_ISREG(st_build.st_mode)) continue;

    if (inv->count >= inv->capacity && inv_grow(inv) != 0) {
      closedir(dir);
      return -1;
    }
    pkg_inventory_entry_t *entry = &inv->entries[inv->count++];
    memset(entry, 0, sizeof(*entry));
    entry->repo_type = repo_type;
    if (snprintf(entry->name, sizeof(entry->name), "%s", de->d_name) <= 0 ||
        snprintf(entry->build_sh, sizeof(entry->build_sh), "%s", build_sh) <= 0) {
      inv->path_errors++;
      closedir(dir);
      return -1;
    }

    if (scan_subpackages(inv, pkg_dir, de->d_name, repo_type) != 0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

int pkg_inventory_scan(pkg_inventory_t *inv, const char *repo_root) {
  struct {
    const char *path;
    pkg_repo_t type;
  } roots[] = {
      {"packages", PKG_REPO_MAIN},
      {"root-packages", PKG_REPO_ROOT},
      {"x11-packages", PKG_REPO_X11},
      {"disabled-packages", PKG_REPO_DISABLED},
  };

  char full[PKG_PATH_MAX];
  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
    int n = snprintf(full, sizeof(full), "%s/%s", repo_root, roots[i].path);
    if (n <= 0 || (size_t)n >= sizeof(full)) {
      inv->path_errors++;
      inv->roots_failed++;
      continue;
    }
    if (scan_repo_root(inv, full, roots[i].type) != 0) continue;
  }

  if (inv->roots_scanned == 0 || inv->count == 0 || inv->roots_failed != 0 ||
      inv->io_errors != 0 || inv->path_errors != 0 ||
      inv->subpackage_scan_failures != 0 || inv->allocation_errors != 0) {
    return -1;
  }
  return 0;
}

void pkg_inventory_destroy(pkg_inventory_t *inv) {
  if (!inv) return;
  free(inv->entries);
  memset(inv, 0, sizeof(*inv));
}

void pkg_inventory_print_json(const pkg_inventory_t *inv) {
  printf("{\"schema\":\"raf.pkg-inventory/v1\",\"status\":\"%s\","
         "\"coverage\":{\"roots_expected\":%u,\"roots_scanned\":%u,"
         "\"roots_failed\":%u,\"io_errors\":%u,\"path_errors\":%u,"
         "\"subpackage_scan_failures\":%u,\"allocation_errors\":%u},"
         "\"count\":%u,\"entries\":[",
         (inv->roots_failed == 0 && inv->io_errors == 0 && inv->path_errors == 0 &&
          inv->subpackage_scan_failures == 0 && inv->allocation_errors == 0 &&
          inv->roots_scanned == inv->roots_expected)
             ? "OBSERVED"
             : "OBSERVED_LIMITED",
         inv->roots_expected, inv->roots_scanned, inv->roots_failed, inv->io_errors,
         inv->path_errors, inv->subpackage_scan_failures, inv->allocation_errors,
         inv->count);
  for (uint32_t i = 0; i < inv->count; ++i) {
    const pkg_inventory_entry_t *e = &inv->entries[i];
    if (i) putchar(',');
    printf("{\"name\":\"%s\",\"repo\":\"%s\",\"build_sh\":\"%s\","
           "\"subpackage\":%s}",
           e->name, repo_name(e->repo_type), e->build_sh,
           e->is_subpackage ? "true" : "false");
  }
  puts("]}");
}
