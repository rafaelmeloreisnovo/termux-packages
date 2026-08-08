#include "pkg_scanner.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

/* ============================================================================
 * REAL: Package Inventory Scanner
 * No mocks, no simulated counts — reads the filesystem directly.
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
  /* NONNULL(1) contract; only capacity check remains */
  if (REAL_UNLIKELY(initial_capacity == 0)) return -1;
  memset(inv, 0, sizeof(*inv));
  inv->entries =
      (pkg_inventory_entry_t *)calloc(initial_capacity, sizeof(*inv->entries));
  if (REAL_UNLIKELY(!inv->entries)) return -1;
  inv->capacity = initial_capacity;
  return 0;
}

REAL_COLD REAL_NOINLINE
static int inv_grow(pkg_inventory_t *inv) {
  uint32_t new_cap = inv->capacity == 0 ? 64 : inv->capacity * 2;
  pkg_inventory_entry_t *n = (pkg_inventory_entry_t *)realloc(
      inv->entries, new_cap * sizeof(*inv->entries));
  if (REAL_UNLIKELY(!n)) return -1;
  memset(n + inv->capacity, 0,
         (new_cap - inv->capacity) * sizeof(*inv->entries));
  inv->entries = n;
  inv->capacity = new_cap;
  return 0;
}

/* Scan a package directory for *.subpackage.sh siblings and append each
 * as a subpackage entry (name derived from filename minus ".subpackage.sh"). */
REAL_HOT
static void scan_subpackages(pkg_inventory_t *inv, const char *pkg_dir,
                             const char *parent_name, pkg_repo_t repo_type) {
  DIR *sd = opendir(pkg_dir);
  if (!sd) return;
  struct dirent *se;
  while ((se = readdir(sd)) != NULL) {
    if (se->d_name[0] == '.') continue;
    size_t len = strlen(se->d_name);
    const char *suffix = ".subpackage.sh";
    size_t suflen = strlen(suffix);
    if (len <= suflen) continue;
    if (strcmp(se->d_name + len - suflen, suffix) != 0) continue;

    char sub_path[PKG_PATH_MAX];
    int n = snprintf(sub_path, sizeof(sub_path), "%s/%s", pkg_dir, se->d_name);
    if (n <= 0 || (size_t)n >= sizeof(sub_path)) continue;

    struct stat st;
    if (stat(sub_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

    if (inv->count >= inv->capacity) {
      if (inv_grow(inv) < 0) { closedir(sd); return; }
    }
    pkg_inventory_entry_t *e = &inv->entries[inv->count];
    size_t nm_len = len - suflen;
    if (nm_len >= PKG_NAME_MAX) nm_len = PKG_NAME_MAX - 1;
    memcpy(e->name, se->d_name, nm_len);
    e->name[nm_len] = '\0';
    strncpy(e->path, sub_path, PKG_PATH_MAX - 1);
    e->path[PKG_PATH_MAX - 1] = '\0';
    strncpy(e->parent, parent_name, PKG_NAME_MAX - 1);
    e->parent[PKG_NAME_MAX - 1] = '\0';
    e->repo = repo_type;
    e->build_sh_size = (uint64_t)st.st_size;
    e->has_build_sh = 1;
    e->is_subpackage = 1;
    inv->count++;
    inv->total_subpackages++;
  }
  closedir(sd);
}

int pkg_inventory_scan_repo(pkg_inventory_t *inv, const char *repo_dir,
                            pkg_repo_t repo_type) {
  /* NONNULL(1,2) contract enforced by compiler */
  DIR *d = opendir(repo_dir);
  if (REAL_UNLIKELY(!d)) return -1;

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (ent->d_name[0] == '.') continue;

    /* Build path to <repo>/<pkg>/build.sh */
    char pkg_dir[PKG_PATH_MAX];
    int nd = snprintf(pkg_dir, sizeof(pkg_dir), "%s/%s", repo_dir, ent->d_name);
    if (nd <= 0 || (size_t)nd >= sizeof(pkg_dir)) continue;

    char build_sh_path[PKG_PATH_MAX];
    int n = snprintf(build_sh_path, sizeof(build_sh_path), "%s/build.sh",
                     pkg_dir);
    if (n <= 0 || (size_t)n >= sizeof(build_sh_path)) continue;

    inv->total_scanned++;

    struct stat st;
    if (stat(build_sh_path, &st) != 0 || !S_ISREG(st.st_mode)) {
      inv->total_missing++;
      continue;
    }

    /* Grow if needed */
    if (inv->count >= inv->capacity) {
      if (inv_grow(inv) < 0) {
        closedir(d);
        return -1;
      }
    }

    pkg_inventory_entry_t *e = &inv->entries[inv->count];
    strncpy(e->name, ent->d_name, PKG_NAME_MAX - 1);
    e->name[PKG_NAME_MAX - 1] = '\0';
    strncpy(e->path, build_sh_path, PKG_PATH_MAX - 1);
    e->path[PKG_PATH_MAX - 1] = '\0';
    strncpy(e->parent, ent->d_name, PKG_NAME_MAX - 1);
    e->parent[PKG_NAME_MAX - 1] = '\0';
    e->repo = repo_type;
    e->build_sh_size = (uint64_t)st.st_size;
    e->has_build_sh = 1;
    e->is_subpackage = 0;

    inv->count++;
    inv->total_with_build_sh++;

    /* Scan subpackages under this pkg dir */
    scan_subpackages(inv, pkg_dir, ent->d_name, repo_type);
  }

  closedir(d);
  return 0;
}

int pkg_inventory_scan_all(pkg_inventory_t *inv, const char *base_dir) {
  /* NONNULL_ALL contract enforced by compiler */
  const struct {
    const char *subdir;
    pkg_repo_t type;
  } repos[] = {
      {"packages",          PKG_REPO_MAIN},
      {"root-packages",     PKG_REPO_ROOT},
      {"x11-packages",      PKG_REPO_X11},
      {"disabled-packages", PKG_REPO_DISABLED},
  };

  for (size_t i = 0; i < sizeof(repos) / sizeof(repos[0]); i++) {
    char full_path[PKG_PATH_MAX];
    int n = snprintf(full_path, sizeof(full_path), "%s/%s", base_dir,
                     repos[i].subdir);
    if (n <= 0 || (size_t)n >= sizeof(full_path)) continue;

    /* Silently skip repos that don't exist — not all forks have all four */
    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

    if (pkg_inventory_scan_repo(inv, full_path, repos[i].type) < 0) {
      return -1;
    }
  }
  return 0;
}

const pkg_inventory_entry_t *
pkg_inventory_find(const pkg_inventory_t *inv, const char *name) {
  /* NONNULL_ALL contract → linker sees dead code eliminated */
  for (uint32_t i = 0; i < inv->count; i++) {
    if (REAL_UNLIKELY(strcmp(inv->entries[i].name, name) == 0)) {
      return &inv->entries[i];
    }
  }
  return NULL;
}

void pkg_inventory_write_json(FILE *out, const pkg_inventory_t *inv) {
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "{\n");
  fprintf(out, "  \"schema\": \"pkg_inventory_v1\",\n");
  fprintf(out, "  \"status\": \"REAL\",\n");
  fprintf(out, "  \"totals\": {\n");
  fprintf(out, "    \"scanned\": %u,\n", inv->total_scanned);
  fprintf(out, "    \"with_build_sh\": %u,\n", inv->total_with_build_sh);
  fprintf(out, "    \"subpackages\": %u,\n", inv->total_subpackages);
  fprintf(out, "    \"missing_build_sh\": %u\n", inv->total_missing);
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
  /* NONNULL_ALL contract enforced by compiler */
  fprintf(out, "=== REAL Package Inventory ===\n");
  fprintf(out, "Directories scanned:       %u\n", inv->total_scanned);
  fprintf(out, "Packages with build.sh:    %u\n", inv->total_with_build_sh);
  fprintf(out, "Subpackages discovered:    %u\n", inv->total_subpackages);
  fprintf(out, "Total entries:             %u\n", inv->count);
  fprintf(out, "Missing build.sh:          %u  [TOKEN_VAZIO]\n",
          inv->total_missing);

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
    fprintf(out, "  %-20s %5u packages, %8" PRIu64 " bytes total\n",
            repo_name((pkg_repo_t)r), by_repo[r], bytes_by_repo[r]);
  }
}

void pkg_inventory_free(pkg_inventory_t *inv) {
  if (!inv) return;
  free(inv->entries);
  memset(inv, 0, sizeof(*inv));
}
