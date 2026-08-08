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
  if ((size_t)new_cap > SIZE_MAX / sizeof(*inv->entries)) {
    inv->allocation_errors++;
    return -1;
  }
  pkg_inventory_entry_t *n = (pkg_inventory_entry_t *)realloc(
      inv->entries, (size_t)new_cap * sizeof(*inv->entries));
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
    if (!S_ISREG(st.st_mode)) {
      inv->non_regular_subpackages++;
      continue;
    }

    if (inv->count >= inv->capacity && inv_grow(inv) < 0) {
      inv->subpackage_scan_failures++;
      closedir(sd);
      return -1;
    }

    pkg_inventory_entry_t *e = &inv->entries[inv->count];
    memcpy(e->name, se->d_name, nm_len);
    e->name[nm_len] = '\0';
    memcpy(e->path, sub_path, (size_t)n + 1U);
    strcpy(e->parent, parent_name);
    e->repo = repo_type;
    e->build_sh_size = (uint64_t)st.st_size;
    e->has_build_sh = 1;
    e->is_subpackage = 1;
    inv->count++;
    inv->total_subpackages++;
  }

  if (closedir(sd) != 0) {
    inv->io_errors++;
    inv->subpackage_scan_failures++;
    return -1;
  }
  return 0;
}

int pkg_inventory_scan_repo(pkg_inventory_t *inv, const char *repo_dir,
                            pkg_repo_t repo_type) {
  DIR *d = opendir(repo_dir);
  if (REAL_UNLIKELY(!d)) {
    inv->io_errors++;
    return -1;
  }

  for (;;) {
    errno = 0;
    struct dirent *ent = readdir(d);
    if (!ent) {
      if (errno != 0) {
        inv->io_errors++;
        closedir(d);
        return -1;
      }
      break;
    }
    if (ent->d_name[0] == '.') continue;

    if (strlen(ent->d_name) >= PKG_NAME_MAX) {
      inv->path_errors++;
      closedir(d);
      return -1;
    }

    char pkg_dir[PKG_PATH_MAX];
    int nd = snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", repo_dir, ent->d_name);
    if (nd <= 0 || (size_t)nd >= sizeof(pkg_dir)) {
      inv->path_errors++;
      closedir(d);
      return -1;
    }

    struct stat pkg_st;
    if (stat(pkg_dir, &pkg_st) != 0) {
      inv->io_errors++;
      closedir(d);
      return -1;
    }
    if (!S_ISDIR(pkg_st.st_mode)) {
      inv->non_directory_entries++;
      continue;
    }

    char build_sh_path[PKG_PATH_MAX];
    int n = snprintf(build_sh_path, sizeof(build_sh_path), "%s/build.sh", pkg_dir);
    if (n <= 0 || (size_t)n >= sizeof(build_sh_path)) {
      inv->path_errors++;
      closedir(d);
      return -1;
    }

    inv->total_scanned++;

    struct stat st;
    if (stat(build_sh_path, &st) != 0) {
      if (errno == ENOENT || errno == ENOTDIR) {
        inv->total_missing++;
        continue;
      }
      inv->io_errors++;
      closedir(d);
      return -1;
    }
    if (!S_ISREG(st.st_mode)) {
      inv->total_missing++;
      continue;
    }

    if (inv->count >= inv->capacity && inv_grow(inv) < 0) {
      closedir(d);
      return -1;
    }

    pkg_inventory_entry_t *e = &inv->entries[inv->count];
    strcpy(e->name, ent->d_name);
    memcpy(e->path, build_sh_path, (size_t)n + 1U);
    strcpy(e->parent, ent->d_name);
    e->repo = repo_type;
    e->build_sh_size = (uint64_t)st.st_size;
    e->has_build_sh = 1;
    e->is_subpackage = 0;
    inv->count++;
    inv->total_with_build_sh++;

    if (scan_subpackages(inv, pkg_dir, ent->d_name, repo_type) < 0) {
      closedir(d);
      return -1;
    }
  }

  if (closedir(d) != 0) {
    inv->io_errors++;
    return -1;
  }
  return 0;
}

int pkg_inventory_scan_all(pkg_inventory_t *inv, const char *base_dir) {
  const struct {
    const char *subdir;
    pkg_repo_t type;
  } repos[] = {
      {"packages",          PKG_REPO_MAIN},
      {"root-packages",     PKG_REPO_ROOT},
      {"x11-packages",      PKG_REPO_X11},
      {"disabled-packages", PKG_REPO_DISABLED},
  };

  inv->roots_expected = (uint32_t)(sizeof(repos) / sizeof(repos[0]));
  inv->roots_present = 0;
  inv->roots_absent = 0;
  inv->roots_failed = 0;

  for (size_t i = 0; i < sizeof(repos) / sizeof(repos[0]); i++) {
    char full_path[PKG_PATH_MAX];
    int n = snprintf(full_path, sizeof(full_path), "%s/%s", base_dir,
                     repos[i].subdir);
    if (n <= 0 || (size_t)n >= sizeof(full_path)) {
      inv->path_errors++;
      inv->roots_failed++;
      return -1;
    }

    struct stat st;
    if (stat(full_path, &st) != 0) {
      if (errno == ENOENT || errno == ENOTDIR) {
        inv->roots_absent++;
        continue;
      }
      inv->io_errors++;
      inv->roots_failed++;
      return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
      inv->roots_failed++;
      return -1;
    }

    inv->roots_present++;
    if (pkg_inventory_scan_repo(inv, full_path, repos[i].type) < 0) {
      inv->roots_failed++;
      return -1;
    }
  }
  return 0;
}

int pkg_inventory_is_complete(const pkg_inventory_t *inv) {
  if (inv->roots_expected == 0 || inv->roots_present != inv->roots_expected)
    return 0;
  if (inv->roots_absent != 0 || inv->roots_failed != 0 ||
      inv->path_errors != 0 || inv->io_errors != 0 ||
      inv->allocation_errors != 0 || inv->subpackage_scan_failures != 0)
    return 0;
  return 1;
}

const pkg_inventory_entry_t *
pkg_inventory_find(const pkg_inventory_t *inv, const char *name) {
  for (uint32_t i = 0; i < inv->count; i++) {
    if (REAL_UNLIKELY(strcmp(inv->entries[i].name, name) == 0))
      return &inv->entries[i];
  }
  return NULL;
}

void pkg_inventory_write_json(FILE *out, const pkg_inventory_t *inv) {
  const int complete = pkg_inventory_is_complete(inv);
  fprintf(out, "{\n");
  fprintf(out, "  \"schema\": \"pkg_inventory/2.0.0\",\n");
  fprintf(out, "  \"status\": \"%s\",\n", complete ? "OBSERVED" : "OBSERVED_LIMITED");
  fprintf(out, "  \"claim_allowed\": false,\n");
  fprintf(out, "  \"coverage_complete\": %s,\n", complete ? "true" : "false");
  fprintf(out, "  \"coverage\": {\n");
  fprintf(out, "    \"roots_expected\": %u,\n", inv->roots_expected);
  fprintf(out, "    \"roots_present\": %u,\n", inv->roots_present);
  fprintf(out, "    \"roots_absent\": %u,\n", inv->roots_absent);
  fprintf(out, "    \"roots_failed\": %u,\n", inv->roots_failed);
  fprintf(out, "    \"path_errors\": %u,\n", inv->path_errors);
  fprintf(out, "    \"io_errors\": %u,\n", inv->io_errors);
  fprintf(out, "    \"allocation_errors\": %u,\n", inv->allocation_errors);
  fprintf(out, "    \"subpackage_scan_failures\": %u\n", inv->subpackage_scan_failures);
  fprintf(out, "  },\n");
  fprintf(out, "  \"totals\": {\n");
  fprintf(out, "    \"scanned\": %u,\n", inv->total_scanned);
  fprintf(out, "    \"with_build_sh\": %u,\n", inv->total_with_build_sh);
  fprintf(out, "    \"subpackages\": %u,\n", inv->total_subpackages);
  fprintf(out, "    \"missing_build_sh\": %u,\n", inv->total_missing);
  fprintf(out, "    \"non_directory_entries\": %u,\n", inv->non_directory_entries);
  fprintf(out, "    \"non_regular_subpackages\": %u\n", inv->non_regular_subpackages);
  fprintf(out, "  },\n");
  fprintf(out, "  \"packages\": [\n");
  for (uint32_t i = 0; i < inv->count; i++) {
    const pkg_inventory_entry_t *e = &inv->entries[i];
    fprintf(out,
            "    {\"name\":\"%s\",\"parent\":\"%s\",\"repo\":\"%s\","
            "\"path\":\"%s\",\"build_sh_size\":%" PRIu64 ","
            "\"is_subpackage\":%s}%s\n",
            e->name, e->parent, repo_name(e->repo), e->path,
            e->build_sh_size, e->is_subpackage ? "true" : "false",
            (i + 1 < inv->count) ? "," : "");
  }
  fprintf(out, "  ]\n");
  fprintf(out, "}\n");
}

void pkg_inventory_report(FILE *out, const pkg_inventory_t *inv) {
  fprintf(out, "=== Package Inventory — OBSERVED%s ===\n",
          pkg_inventory_is_complete(inv) ? "" : "_LIMITED");
  fprintf(out, "Claim allowed:             false\n");
  fprintf(out, "Known roots:               %u/%u present, %u absent, %u failed\n",
          inv->roots_present, inv->roots_expected, inv->roots_absent,
          inv->roots_failed);
  fprintf(out, "Directories scanned:       %u\n", inv->total_scanned);
  fprintf(out, "Packages with build.sh:    %u\n", inv->total_with_build_sh);
  fprintf(out, "Subpackages discovered:    %u\n", inv->total_subpackages);
  fprintf(out, "Total entries:             %u\n", inv->count);
  fprintf(out, "Missing build.sh:          %u\n", inv->total_missing);
  fprintf(out, "Collection errors:         path=%u io=%u alloc=%u subpkg=%u\n",
          inv->path_errors, inv->io_errors, inv->allocation_errors,
          inv->subpackage_scan_failures);

  uint32_t by_repo[4] = {0};
  uint64_t bytes_by_repo[4] = {0};
  for (uint32_t i = 0; i < inv->count; i++) {
    pkg_repo_t r = inv->entries[i].repo;
    if (r < 4) {
      by_repo[r]++;
      bytes_by_repo[r] += inv->entries[i].build_sh_size;
    }
  }
  for (int r = 0; r < 4; r++) {
    fprintf(out, "  %-20s %5u entries, %8" PRIu64 " bytes total\n",
            repo_name((pkg_repo_t)r), by_repo[r], bytes_by_repo[r]);
  }
}

void pkg_inventory_free(pkg_inventory_t *inv) {
  if (!inv) return;
  free(inv->entries);
  memset(inv, 0, sizeof(*inv));
}
