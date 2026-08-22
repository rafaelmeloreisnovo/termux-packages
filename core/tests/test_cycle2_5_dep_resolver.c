#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

/* Forward declarations */
#include "../manifest_v2.h"

/*
 * TV-05 Test Gate: DEP_GRAPH (Dependency graph validation from manifest)
 *
 * Closure criteria:
 * - termux_resolve_dependencies callable (interface exists)
 * - All dependency indices within bounds (0 to entry_count-1)
 * - No circular dependencies detected (A→B→C is OK; A→B→A is FAIL)
 * - Proper error handling for invalid indices (returns non-zero)
 * - Proper error handling for circular references (returns non-zero)
 * - Test exits with code 0 on PASS, non-zero on FAIL
 */

int main(void) {
  int failures = 0;

  fprintf(stdout, "[TV-05] DEP_GRAPH test gate\n");
  fprintf(stdout, "[TV-05] Testing termux dependency resolution interface\n");

  /* Test 1: NULL entries should return error */
  fprintf(stdout, "\n[TEST 1] NULL entries\n");
  int ret1 = termux_resolve_manifest_dependencies(NULL, 5);
  if (ret1 != 0) {
    fprintf(stdout, "  [PASS] Returned non-zero (%d) for NULL entries\n", ret1);
  } else {
    fprintf(stdout, "  [FAIL] Expected non-zero, got %d\n", ret1);
    failures++;
  }

  /* Test 2: Zero entry count should succeed */
  fprintf(stdout, "\n[TEST 2] Zero entry count\n");
  struct termux_manifest_entry_v2 entries_empty[1];
  int ret2 = termux_resolve_manifest_dependencies(entries_empty, 0);
  if (ret2 == 0) {
    fprintf(stdout, "  [PASS] Returned 0 for zero entries\n");
  } else {
    fprintf(stdout, "  [FAIL] Expected 0, got %d\n", ret2);
    failures++;
  }

  /* Test 3: Simple valid dependency chain (no cycles) */
  fprintf(stdout, "\n[TEST 3] Valid linear dependency chain\n");
  struct termux_manifest_entry_v2 entries_linear[3];
  memset(entries_linear, 0, sizeof(entries_linear));

  /* Entry 0: no dependencies */
  entries_linear[0].dep_count = 0;

  /* Entry 1: depends on Entry 0 */
  entries_linear[1].dep_count = 1;
  entries_linear[1].deps[0] = 0;

  /* Entry 2: depends on Entry 1 */
  entries_linear[2].dep_count = 1;
  entries_linear[2].deps[0] = 1;

  int ret3 = termux_resolve_manifest_dependencies(entries_linear, 3);
  if (ret3 == 0) {
    fprintf(stdout, "  [PASS] Valid chain resolved (exit=%d)\n", ret3);
  } else {
    fprintf(stdout, "  [FAIL] Expected 0, got %d\n", ret3);
    failures++;
  }

  /* Test 4: Out-of-bounds dependency index should fail */
  fprintf(stdout, "\n[TEST 4] Out-of-bounds dependency index\n");
  struct termux_manifest_entry_v2 entries_oob[2];
  memset(entries_oob, 0, sizeof(entries_oob));

  entries_oob[0].dep_count = 0;

  /* Entry 1: depends on index 5 (out of bounds) */
  entries_oob[1].dep_count = 1;
  entries_oob[1].deps[0] = 5;

  int ret4 = termux_resolve_manifest_dependencies(entries_oob, 2);
  if (ret4 != 0) {
    fprintf(stdout, "  [PASS] Rejected out-of-bounds index (exit=%d)\n", ret4);
  } else {
    fprintf(stdout, "  [FAIL] Expected non-zero, got %d\n", ret4);
    failures++;
  }

  /* Test 5: Circular dependency should be detected */
  fprintf(stdout, "\n[TEST 5] Circular dependency detection\n");
  struct termux_manifest_entry_v2 entries_circular[2];
  memset(entries_circular, 0, sizeof(entries_circular));

  /* Entry 0: depends on Entry 1 */
  entries_circular[0].dep_count = 1;
  entries_circular[0].deps[0] = 1;

  /* Entry 1: depends on Entry 0 (cycle!) */
  entries_circular[1].dep_count = 1;
  entries_circular[1].deps[0] = 0;

  int ret5 = termux_detect_manifest_circular_deps(entries_circular, 2);
  if (ret5 != 0) {
    fprintf(stdout, "  [PASS] Detected circular dependency (exit=%d)\n", ret5);
  } else {
    fprintf(stdout, "  [FAIL] Expected non-zero, got %d\n", ret5);
    failures++;
  }

  /* Test 6: Complex valid DAG (directed acyclic graph) */
  fprintf(stdout, "\n[TEST 6] Complex valid DAG\n");
  struct termux_manifest_entry_v2 entries_dag[5];
  memset(entries_dag, 0, sizeof(entries_dag));

  /* Entry 0: no dependencies */
  entries_dag[0].dep_count = 0;

  /* Entry 1: depends on 0 */
  entries_dag[1].dep_count = 1;
  entries_dag[1].deps[0] = 0;

  /* Entry 2: depends on 0 */
  entries_dag[2].dep_count = 1;
  entries_dag[2].deps[0] = 0;

  /* Entry 3: depends on 1 and 2 */
  entries_dag[3].dep_count = 2;
  entries_dag[3].deps[0] = 1;
  entries_dag[3].deps[1] = 2;

  /* Entry 4: depends on 3 */
  entries_dag[4].dep_count = 1;
  entries_dag[4].deps[0] = 3;

  int ret6 = termux_resolve_manifest_dependencies(entries_dag, 5);
  if (ret6 == 0) {
    fprintf(stdout, "  [PASS] Complex DAG resolved (exit=%d)\n", ret6);
  } else {
    fprintf(stdout, "  [FAIL] Expected 0, got %d\n", ret6);
    failures++;
  }

  /* Test 7: Self-referential dependency (self-cycle) */
  fprintf(stdout, "\n[TEST 7] Self-referential dependency\n");
  struct termux_manifest_entry_v2 entries_self[1];
  memset(entries_self, 0, sizeof(entries_self));

  /* Entry 0: depends on itself */
  entries_self[0].dep_count = 1;
  entries_self[0].deps[0] = 0;

  int ret7 = termux_detect_manifest_circular_deps(entries_self, 1);
  if (ret7 != 0) {
    fprintf(stdout, "  [PASS] Detected self-cycle (exit=%d)\n", ret7);
  } else {
    fprintf(stdout, "  [FAIL] Expected non-zero, got %d\n", ret7);
    failures++;
  }

  /* Summary */
  fprintf(stdout, "\n[TV-05] SUMMARY\n");
  fprintf(stdout, "  Tests run: 7\n");
  fprintf(stdout, "  Failures: %d\n", failures);
  fprintf(stdout, "  Status: %s\n", failures == 0 ? "PASS" : "FAIL");
  fprintf(stdout, "\n[TV-05] Closure criteria:\n");
  fprintf(stdout, "  - termux_resolve_manifest_dependencies interface exists: YES\n");
  fprintf(stdout, "  - Bounds checking on dependency indices: %s\n",
          ret4 != 0 ? "YES" : "NO");
  fprintf(stdout, "  - Circular dependency detection: %s\n",
          ret5 != 0 ? "YES" : "NO");
  fprintf(stdout, "  - Complex DAG resolution: %s\n",
          ret6 == 0 ? "YES" : "NO");

  return failures > 0 ? 1 : 0;
}
