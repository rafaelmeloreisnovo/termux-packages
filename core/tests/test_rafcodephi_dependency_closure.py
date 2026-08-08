#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "rafcodephi_dependency_closure.py"

spec = importlib.util.spec_from_file_location("rafcodephi_dependency_closure", SCRIPT)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class DependencyClosureTests(unittest.TestCase):
    def write_pkg(self, repo: Path, name: str, depends: str = "", build_depends: str = "") -> None:
        pkg = repo / "packages" / name
        pkg.mkdir(parents=True, exist_ok=True)
        text = ["TERMUX_PKG_VERSION=1"]
        if depends:
            text.append(f'TERMUX_PKG_DEPENDS="{depends}"')
        if build_depends:
            text.append(f'TERMUX_PKG_BUILD_DEPENDS="{build_depends}"')
        (pkg / "build.sh").write_text("\n".join(text) + "\n", encoding="utf-8")

    def test_transitive_literal_closure(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            self.write_pkg(repo, "bash", "readline, termux-tools")
            self.write_pkg(repo, "readline", "ncurses")
            self.write_pkg(repo, "termux-tools")
            self.write_pkg(repo, "ncurses")
            result = mod.build_closure(repo, "bash")
            self.assertEqual(result["closure"], ["bash", "ncurses", "readline", "termux-tools"])
            self.assertEqual(result["unresolved_count"], 0)
            self.assertEqual(result["dynamic_dependency_package_count"], 0)
            self.assertTrue(result["complete_for_literal_projection"])
            self.assertEqual(result["artifact_build_rule"], "do_not_use_-I_with_custom_TERMUX_APP_PACKAGE")

    def test_first_alternative_is_explicit_projection(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            self.write_pkg(repo, "root", "a | b")
            self.write_pkg(repo, "a")
            self.write_pkg(repo, "b")
            result = mod.build_closure(repo, "root")
            self.assertIn("a", result["closure"])
            self.assertNotIn("b", result["closure"])
            self.assertEqual(result["projection"], "literal_first_alternative_TERMUX_PKG_DEPENDS_and_BUILD_DEPENDS")

    def test_missing_dependency_remains_unresolved(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            self.write_pkg(repo, "root", "missing-package")
            result = mod.build_closure(repo, "root")
            self.assertEqual(result["unresolved"], ["missing-package"])
            self.assertFalse(result["complete_for_literal_projection"])

    def test_dynamic_dependency_is_not_guessed(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            self.write_pkg(repo, "root", "$SOME_DEP")
            result = mod.build_closure(repo, "root")
            self.assertIn("root", result["dynamic_dependency_fields"])
            self.assertFalse(result["complete_for_literal_projection"])

    def test_build_dependencies_are_included(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            self.write_pkg(repo, "root", build_depends="toolchain-helper")
            self.write_pkg(repo, "toolchain-helper")
            result = mod.build_closure(repo, "root")
            self.assertIn("toolchain-helper", result["closure"])


if __name__ == "__main__":
    unittest.main()
