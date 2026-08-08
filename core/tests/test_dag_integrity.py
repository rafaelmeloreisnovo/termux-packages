#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "core"

HARNESS = r'''
#include "pkg_dag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 3) return 64;
    const char *mode = argv[1];
    const char *base = argv[2];
    const char *names[] = {"a", "b", "c", "d", "e"};
    pkg_inventory_t inv;
    memset(&inv, 0, sizeof(inv));
    inv.count = 5;
    inv.capacity = 5;
    inv.entries = calloc(inv.count, sizeof(*inv.entries));
    if (!inv.entries) return 65;

    for (uint32_t i = 0; i < inv.count; i++) {
        snprintf(inv.entries[i].name, sizeof(inv.entries[i].name), "%s", names[i]);
        snprintf(inv.entries[i].parent, sizeof(inv.entries[i].parent), "%s", names[i]);
        snprintf(inv.entries[i].path, sizeof(inv.entries[i].path), "%s/%s.sh", base, names[i]);
        inv.entries[i].has_build_sh = 1;
    }

    pkg_dag_t dag;
    int build_rc = pkg_dag_build(&dag, &inv);
    int topo_rc = -99;
    if (build_rc == 0) topo_rc = pkg_dag_topo_sort(&dag);
    printf("mode=%s build=%d topo=%d edges=%u unresolved=%u alt=%u overflow=%u alloc=%u scc=%u cycle_nodes=%u topo_count=%u\n",
           mode, build_rc, topo_rc, dag.edge_count, dag.unresolved_count,
           dag.alternative_dep_fields, dag.dependency_field_overflows,
           dag.allocation_failures, dag.cycle_count, dag.cycle_nodes, dag.topo_count);
    pkg_dag_free(&dag);
    free(inv.entries);
    return 0;
}
'''


class DagIntegrityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory()
        td = Path(cls.tmp.name)
        src = td / "harness.c"
        cls.bin = td / "dag-harness"
        src.write_text(HARNESS, encoding="utf-8")
        proc = subprocess.run(
            [
                "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(CORE), str(src), str(CORE / "pkg_dag.c"),
                str(CORE / "pkg_parser.c"), "-o", str(cls.bin),
            ],
            capture_output=True, text=True, check=False,
        )
        if proc.returncode != 0:
            raise RuntimeError(proc.stderr)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    @staticmethod
    def write_pkg(base: Path, name: str, depends: str) -> None:
        text = f'TERMUX_PKG_VERSION=1\nTERMUX_PKG_DEPENDS="{depends}"\n'
        (base / f"{name}.sh").write_text(text, encoding="utf-8")

    def run_harness(self, mode: str, base: Path) -> str:
        proc = subprocess.run(
            [str(self.bin), mode, str(base)],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return proc.stdout.strip()

    def test_two_cycles_are_two_cyclic_sccs_not_one_aggregate(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.write_pkg(base, "a", "b")
            self.write_pkg(base, "b", "a")
            self.write_pkg(base, "c", "d")
            self.write_pkg(base, "d", "c")
            self.write_pkg(base, "e", "a | external-fallback")
            out = self.run_harness("scc", base)
            self.assertIn("build=0 topo=0", out)
            self.assertIn("edges=5", out)
            self.assertIn("alt=1", out)
            self.assertIn("overflow=0", out)
            self.assertIn("alloc=0", out)
            self.assertIn("scc=2", out)
            self.assertIn("cycle_nodes=4", out)

    def test_long_dependency_field_fails_instead_of_truncating(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            long_name = "z" * 140
            self.write_pkg(base, "a", long_name)
            for name in ("b", "c", "d", "e"):
                self.write_pkg(base, name, "")
            out = self.run_harness("overflow", base)
            self.assertIn("build=-1", out)
            self.assertIn("overflow=1", out)
            self.assertIn("alloc=0", out)

    def test_acyclic_projection_orders_all_nodes(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.write_pkg(base, "a", "")
            self.write_pkg(base, "b", "a")
            self.write_pkg(base, "c", "b")
            self.write_pkg(base, "d", "c")
            self.write_pkg(base, "e", "d")
            out = self.run_harness("acyclic", base)
            self.assertIn("build=0 topo=0", out)
            self.assertIn("scc=0", out)
            self.assertIn("cycle_nodes=0", out)
            self.assertIn("topo_count=5", out)


if __name__ == "__main__":
    unittest.main()
