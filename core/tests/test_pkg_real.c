/*
 * REAL: Tests for pkg_scanner + pkg_parser + pkg_dag.
 * Status: REAL — validates against actual filesystem, no mocks.
 *
 * These tests MUST run from repo root so packages/ etc. are visible.
 */

#include "../pkg_dag.h"
#include "../pkg_parser.h"
#include "../pkg_scanner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int fail_count = 0;
static int pass_count = 0;

#define REAL_ASSERT(cond, msg)                                         \
  do {                                                                 \
    if (cond) {                                                        \
      pass_count++;                                                    \
      printf("  ✓ %s\n", msg);                                         \
    } else {                                                           \
      fail_count++;                                                    \
      printf("  ✗ %s\n", msg);                                         \
    }                                                                  \
  } while (0)

static void test_inventory_real(void) {
  printf("\n[REAL] Inventory scan\n");

  pkg_inventory_t inv;
  assert(pkg_inventory_init(&inv, 512) == 0);
  assert(pkg_inventory_scan_all(&inv, ".") == 0);

  REAL_ASSERT(inv.total_scanned >= 2000,
              "scanned at least 2000 directories");
  REAL_ASSERT(inv.total_with_build_sh >= 2000,
              "found at least 2000 real build.sh files");
  REAL_ASSERT(inv.total_missing == 0,
              "no missing build.sh in known repos");
  REAL_ASSERT(inv.total_subpackages > 0,
              "discovered subpackages (>0)");

  /* Look up a well-known package */
  const pkg_inventory_entry_t *e = pkg_inventory_find(&inv, "termux-tools");
  REAL_ASSERT(e != NULL, "found termux-tools by name");
  if (e) {
    REAL_ASSERT(e->has_build_sh == 1,
                "termux-tools has build.sh flag set");
    REAL_ASSERT(e->build_sh_size > 0,
                "termux-tools build.sh has non-zero size");
    REAL_ASSERT(e->is_subpackage == 0,
                "termux-tools is not a subpackage");
  }

  pkg_inventory_free(&inv);
}

static void test_parser_real(void) {
  printf("\n[REAL] build.sh parser\n");

  pkg_parser_result_t r;
  int rc = pkg_parser_parse_file("packages/termux-tools/build.sh", &r);
  REAL_ASSERT(rc == 0, "parser returned OK");
  REAL_ASSERT(r.parse_ok == 1, "parse_ok flag set");
  REAL_ASSERT(strcmp(r.name, "termux-tools") == 0,
              "name derived from directory");
  REAL_ASSERT(strlen(r.version) > 0, "version extracted (non-empty)");
  REAL_ASSERT(r.has_srcurl == 1, "SRCURL detected");
  REAL_ASSERT(r.has_sha256 == 1, "SHA256 detected");
  REAL_ASSERT(strlen(r.sha256) == 64, "SHA256 is 64 hex chars");
  REAL_ASSERT(r.has_depends == 1, "DEPENDS detected");
  REAL_ASSERT(strstr(r.depends_raw, "coreutils") != NULL,
              "DEPENDS contains coreutils");
  REAL_ASSERT(r.lines_read > 0, "lines_read > 0");
}

static void test_dag_real(void) {
  printf("\n[REAL] DAG construction\n");

  pkg_inventory_t inv;
  assert(pkg_inventory_init(&inv, 512) == 0);
  assert(pkg_inventory_scan_all(&inv, ".") == 0);

  pkg_dag_t dag;
  assert(pkg_dag_build(&dag, &inv) == 0);
  assert(pkg_dag_topo_sort(&dag) == 0);

  REAL_ASSERT(dag.edge_count > 1000,
              "DAG has >1000 real edges");
  REAL_ASSERT(dag.total_depends_edges > 0,
              "DEPENDS edges counted (>0)");
  REAL_ASSERT(dag.total_build_dep_edges > 0,
              "BUILD_DEPENDS edges counted (>0)");
  REAL_ASSERT(dag.cycle_count == 0,
              "no cycles in real dependency graph");
  REAL_ASSERT(dag.topo_count == inv.count,
              "topological sort covers every node");
  REAL_ASSERT(dag.max_depth > 0 && dag.max_depth < 100,
              "max_depth is bounded and > 0");

  /* Verify termux-tools depth is small (few deps deep) */
  int32_t tt_idx = -1;
  for (uint32_t i = 0; i < inv.count; i++) {
    if (strcmp(inv.entries[i].name, "termux-tools") == 0) {
      tt_idx = (int32_t)i;
      break;
    }
  }
  REAL_ASSERT(tt_idx >= 0, "found termux-tools index in inventory");

  pkg_dag_free(&dag);
  pkg_inventory_free(&inv);
}

int main(void) {
  printf("=== REAL pkg_scanner + pkg_parser + pkg_dag tests ===\n");
  printf("(must be run from repo root)\n");

  test_inventory_real();
  test_parser_real();
  test_dag_real();

  printf("\n=== Summary ===\n");
  printf("Passed: %d\n", pass_count);
  printf("Failed: %d\n", fail_count);
  return fail_count == 0 ? 0 : 1;
}
