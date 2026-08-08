#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "core"

HARNESS = r'''
#include "pkg_scanner.h"
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) return 64;
    pkg_inventory_t inv;
    if (pkg_inventory_init(&inv, 2) != 0) return 65;
    int rc = pkg_inventory_scan_all(&inv, argv[1]);
    printf("rc=%d roots=%u/%u absent=%u failed=%u complete=%d count=%u sub=%u path=%u io=%u alloc=%u subfail=%u\n",
           rc, inv.roots_present, inv.roots_expected, inv.roots_absent,
           inv.roots_failed, pkg_inventory_is_complete(&inv), inv.count,
           inv.total_subpackages, inv.path_errors, inv.io_errors,
           inv.allocation_errors, inv.subpackage_scan_failures);
    pkg_inventory_free(&inv);
    return 0;
}
'''


class ScannerCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory()
        td = Path(cls.tmp.name)
        cls.src = td / "harness.c"
        cls.bin = td / "scanner-harness"
        cls.src.write_text(HARNESS, encoding="utf-8")
        proc = subprocess.run(
            [
                "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(CORE), str(cls.src), str(CORE / "pkg_scanner.c"),
                "-o", str(cls.bin),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if proc.returncode != 0:
            raise RuntimeError(proc.stderr)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def make_repo(self, base: Path, include_all: bool = True) -> None:
        roots = ["packages", "root-packages", "x11-packages", "disabled-packages"]
        if not include_all:
            roots.remove("x11-packages")
        for root in roots:
            pkg = base / root / f"pkg-{root}"
            pkg.mkdir(parents=True)
            (pkg / "build.sh").write_text("TERMUX_PKG_VERSION=1\n", encoding="utf-8")
        main_pkg = base / "packages" / "pkg-packages"
        (main_pkg / "demo.subpackage.sh").write_text("TERMUX_SUBPKG_DESCRIPTION=x\n", encoding="utf-8")

    def run_harness(self, base: Path | str) -> str:
        proc = subprocess.run(
            [str(self.bin), str(base)], capture_output=True, text=True, check=False
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return proc.stdout.strip()

    def test_complete_four_root_inventory_is_explicit(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.make_repo(base)
            out = self.run_harness(base)
            self.assertIn("rc=0", out)
            self.assertIn("roots=4/4", out)
            self.assertIn("absent=0", out)
            self.assertIn("failed=0", out)
            self.assertIn("complete=1", out)
            self.assertIn("count=5", out)
            self.assertIn("sub=1", out)
            self.assertIn("path=0 io=0 alloc=0 subfail=0", out)

    def test_missing_known_root_is_visible_not_silent(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.make_repo(base, include_all=False)
            out = self.run_harness(base)
            self.assertIn("rc=0", out)
            self.assertIn("roots=3/4", out)
            self.assertIn("absent=1", out)
            self.assertIn("complete=0", out)

    def test_known_root_that_is_file_is_failure(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            self.make_repo(base)
            target = base / "x11-packages"
            for child in target.rglob("*"):
                if child.is_file():
                    child.unlink()
            for child in sorted(target.rglob("*"), reverse=True):
                if child.is_dir():
                    child.rmdir()
            target.rmdir()
            target.write_text("not a directory", encoding="utf-8")
            out = self.run_harness(base)
            self.assertIn("rc=-1", out)
            self.assertIn("failed=1", out)
            self.assertIn("complete=0", out)

    def test_overlong_base_path_fails_closed(self) -> None:
        out = self.run_harness("x" * 600)
        self.assertIn("rc=-1", out)
        self.assertIn("path=1", out)
        self.assertIn("complete=0", out)


if __name__ == "__main__":
    unittest.main()
