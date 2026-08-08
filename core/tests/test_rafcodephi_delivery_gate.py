#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "rafcodephi_delivery_gate.py"

spec = importlib.util.spec_from_file_location("rafcodephi_delivery_gate", SCRIPT)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class RafcodephiDeliveryGateTests(unittest.TestCase):
    def test_source_gate_uses_existing_builder_and_essential_recipes(self) -> None:
        doc = mod.source_gate(ROOT)
        self.assertEqual(doc["state"], "PASS")
        self.assertFalse(doc["claim_allowed"])
        self.assertEqual(doc["package"], "com.termux.rafacodephi")
        self.assertEqual(doc["prefix"], "/data/data/com.termux.rafacodephi/files/usr")
        self.assertEqual(doc["architectures"], ["aarch64", "arm"])
        self.assertEqual(doc["package_format"], "debian")
        self.assertEqual(doc["source_contract"]["bash_recipe"], "PASS")
        self.assertEqual(doc["source_contract"]["termux_tools_recipe"], "PASS")
        self.assertEqual(doc["artifact"], "NOT_MEASURED")
        self.assertEqual(doc["physical_device_runtime"], "NOT_MEASURED")

    def test_source_manifest_hashes_are_complete_sha256(self) -> None:
        doc = mod.source_gate(ROOT)
        self.assertGreaterEqual(len(doc["sources"]), 5)
        for name, record in doc["sources"].items():
            with self.subTest(name=name):
                digest = record["sha256"]
                self.assertEqual(len(digest), 64)
                int(digest, 16)

    def test_missing_artifact_is_blocked_not_promoted(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            missing = Path(td) / "bash.deb"
            with self.assertRaises(mod.GateError) as ctx:
                mod.artifact_gate(ROOT, missing, "bash", "aarch64")
            self.assertIn("artifact missing", str(ctx.exception))

    def test_unsupported_arch_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            artifact = Path(td) / "fake.deb"
            artifact.write_bytes(b"not-a-deb")
            with self.assertRaises(mod.GateError) as ctx:
                mod.artifact_gate(ROOT, artifact, "bash", "x86_64")
            self.assertIn("unsupported RAFCODEPHI delivery arch", str(ctx.exception))

    def test_source_gate_rejects_drifted_copy(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            clone = Path(td)
            for rel in (
                "build-package.sh",
                "packages/bash/build.sh",
                "packages/termux-tools/build.sh",
                "scripts/emit_rafcodephi_bootstrap_source_manifest.py",
                "repo.json",
            ):
                src = ROOT / rel
                dst = clone / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                dst.write_bytes(src.read_bytes())
            bash_path = clone / "packages/bash/build.sh"
            text = bash_path.read_text(encoding="utf-8").replace(
                "TERMUX_PKG_ESSENTIAL=true", "TERMUX_PKG_ESSENTIAL=false", 1
            )
            bash_path.write_text(text, encoding="utf-8")
            with self.assertRaises(mod.GateError) as ctx:
                mod.source_gate(clone)
            self.assertIn("bash essential flag", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
