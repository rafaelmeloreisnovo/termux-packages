/*
 * REAL: CLI for real inventory + parse + DAG.
 * Status: REAL — produces authoritative JSON and human report.
 *
 * Usage:
 *   pkg-real inventory <base_dir> [--json]
 *   pkg-real parse <build_sh_path> [--json]
 *   pkg-real dag <base_dir> [--json]
 *
 * Example:
 *   ./pkg-real inventory .            # scan repo, print report
 *   ./pkg-real dag . > dag.json       # full DAG in JSON
 */

#include "pkg_dag.h"
#include "pkg_parser.h"
#include "pkg_scanner.h"
#include <stdio.h>
#include <string.h>

static void usage(void) {
  fprintf(stderr,
          "Usage:\n"
          "  pkg-real inventory <base_dir> [--json]\n"
          "  pkg-real parse <build_sh_path> [--json]\n"
          "  pkg-real dag <base_dir> [--json]\n");
}

static int is_json_flag(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0) return 1;
  }
  return 0;
}

static int cmd_inventory(const char *base_dir, int json) {
  pkg_inventory_t inv;
  if (pkg_inventory_init(&inv, 512) < 0) {
    fprintf(stderr, "REAL_ERROR: inventory init failed\n");
    return 2;
  }
  if (pkg_inventory_scan_all(&inv, base_dir) < 0) {
    fprintf(stderr, "REAL_ERROR: scan failed for %s\n", base_dir);
    pkg_inventory_free(&inv);
    return 2;
  }
  if (json) pkg_inventory_write_json(stdout, &inv);
  else      pkg_inventory_report(stdout, &inv);
  pkg_inventory_free(&inv);
  return 0;
}

static int cmd_parse(const char *build_sh_path, int json) {
  pkg_parser_result_t r;
  if (pkg_parser_parse_file(build_sh_path, &r) < 0) {
    fprintf(stderr, "REAL_ERROR: parse failed for %s\n", build_sh_path);
    return 2;
  }
  if (json) pkg_parser_write_json(stdout, &r);
  else      pkg_parser_report(stdout, &r);
  return 0;
}

static int cmd_dag(const char *base_dir, int json) {
  pkg_inventory_t inv;
  if (pkg_inventory_init(&inv, 512) < 0) return 2;
  if (pkg_inventory_scan_all(&inv, base_dir) < 0) {
    pkg_inventory_free(&inv);
    return 2;
  }
  pkg_dag_t dag;
  if (pkg_dag_build(&dag, &inv) < 0) {
    fprintf(stderr, "REAL_ERROR: dag build failed\n");
    /* C29 fix: pkg_dag_build may have partially populated dag (parsed
     * array, some edges) before returning -1. Call pkg_dag_free to
     * release those. Process exit would reclaim anyway, but library
     * use cases and repeated CLI invocations would leak (up to
     * ~430 MB of parsed data on a 3000-package repo). */
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }
  if (pkg_dag_topo_sort(&dag) < 0) {
    fprintf(stderr, "REAL_ERROR: topo sort failed\n");
    pkg_dag_free(&dag);
    pkg_inventory_free(&inv);
    return 2;
  }
  if (json) pkg_dag_write_json(stdout, &dag);
  else {
    pkg_inventory_report(stdout, &inv);
    fputc('\n', stdout);
    pkg_dag_report(stdout, &dag);
  }
  pkg_dag_free(&dag);
  pkg_inventory_free(&inv);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) { usage(); return 1; }
  int json = is_json_flag(argc, argv);

  if (strcmp(argv[1], "inventory") == 0) {
    return cmd_inventory(argv[2], json);
  } else if (strcmp(argv[1], "parse") == 0) {
    return cmd_parse(argv[2], json);
  } else if (strcmp(argv[1], "dag") == 0) {
    return cmd_dag(argv[2], json);
  }
  usage();
  return 1;
}
