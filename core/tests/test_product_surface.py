#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SURFACE = ROOT / "core" / "product_surface.v1.json"
INSTALLER = ROOT / "scripts" / "install_core_governed.sh"
MAKEFILE = ROOT / "core" / "Makefile"
PREFIX = "/data/data/com.termux.rafacodephi/files/usr"


class ProductSurfaceTests(unittest.TestCase):
    def surface(self) -> dict:
        return json.loads(SURFACE.read_text(encoding="utf-8"))

    def test_contract_has_minimal_non_test_surface(self) -> None:
        doc = self.surface()
        self.assertEqual(doc["schema"], "rafcodephi_core_product_surface/1.0.0")
        self.assertFalse(doc["claim_allowed"])
        self.assertEqual(doc["allowed_binaries"], ["termux-build-core", "manifest-dumper"])
        for name in doc["allowed_binaries"]:
            self.assertFalse(name.startswith("test-"))
            self.assertNotIn("benchmark", name)
            self.assertNotIn("fixture", name)

    def test_forbidden_sources_cover_known_risky_surfaces(self) -> None:
        forbidden = set(self.surface()["forbidden_product_sources"])
        self.assertTrue(
            {
                "crypto_ed25519.c",
                "crypto_chacha20.c",
                "crypto_pqc.c",
                "gpu_integration.c",
                "dist_orchestrator.c",
                "rpc_coordinator.c",
            }.issubset(forbidden)
        )

    def test_installer_never_calls_legacy_make_install(self) -> None:
        text = INSTALLER.read_text(encoding="utf-8")
        self.assertNotIn("make -C \"$CORE_DIR\" install", text)
        self.assertIn('make -C "$CORE_DIR" "${ALLOWED[@]}"', text)
        self.assertIn("core/product_surface.v1.json", text)

    def test_legacy_install_is_detected_as_broad_but_not_governed(self) -> None:
        text = MAKEFILE.read_text(encoding="utf-8")
        self.assertIn("install: $(TARGETS)", text)
        doc = self.surface()
        self.assertEqual(doc["governed_installer"], "scripts/install_core_governed.sh")
        self.assertFalse(doc["invariants"]["build_all_is_product_surface"])

    def test_isolated_governed_install_matches_exact_declared_tree(self) -> None:
        doc = self.surface()
        env = os.environ.copy()
        env["PREFIX"] = PREFIX
        with tempfile.TemporaryDirectory() as td:
            env["DESTDIR"] = td
            proc = subprocess.run(
                ["bash", str(INSTALLER)],
                cwd=ROOT,
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, proc.stderr + proc.stdout)
            self.assertIn("GOVERNED_INSTALL=PASS", proc.stdout)
            self.assertIn("ANDROID_RUNTIME=NOT_MEASURED", proc.stdout)
            installed = Path(td) / PREFIX.lstrip("/") / "bin"
            actual = sorted(p.name for p in installed.iterdir() if p.is_file())
            self.assertEqual(actual, sorted(doc["allowed_binaries"]))
            for path in installed.iterdir():
                self.assertTrue(os.access(path, os.X_OK))


if __name__ == "__main__":
    unittest.main()
