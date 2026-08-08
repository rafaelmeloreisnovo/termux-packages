#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "rafcodephi_apt_repo_gate.py"

spec = importlib.util.spec_from_file_location("rafcodephi_apt_repo_gate", SCRIPT)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


@unittest.skipUnless(shutil.which("dpkg-deb"), "dpkg-deb required")
class AptRepoGateTests(unittest.TestCase):
    def make_deb(self, base: Path, name: str = "bash", version: str = "1.0", arch: str = "aarch64") -> Path:
        tree = base / f"src-{name}"
        debian = tree / "DEBIAN"
        binary = tree / "data" / "usr" / "bin"
        debian.mkdir(parents=True)
        binary.mkdir(parents=True)
        (debian / "control").write_text(
            "\n".join(
                [
                    f"Package: {name}",
                    f"Version: {version}",
                    f"Architecture: {arch}",
                    "Maintainer: RAFCODEPHI fixture <fixture@example.invalid>",
                    "Description: D4 metadata fixture only",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        (binary / name).write_text("fixture\n", encoding="utf-8")
        out = base / f"{name}_{version}_{arch}.deb"
        proc = subprocess.run(
            ["dpkg-deb", "--build", "--root-owner-group", str(tree), str(out)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return out

    def make_d3_receipt(self, receipts: Path, deb: Path) -> Path:
        receipts.mkdir(parents=True, exist_ok=True)
        doc = {
            "schema": "rafcodephi_delivery_gate/1.1.0",
            "gate": "artifact",
            "state": "PASS",
            "claim_allowed": False,
            "artifact_sha256": mod.sha256_file(deb),
            "package": mod.dpkg_field(deb, "Package"),
            "version": mod.dpkg_field(deb, "Version"),
            "arch": "aarch64",
            "debian_architecture": mod.dpkg_field(deb, "Architecture"),
        }
        path = receipts / "d3.json"
        path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
        return path

    def test_d4_requires_matching_d3_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            artifacts = base / "artifacts"
            receipts = base / "receipts"
            artifacts.mkdir()
            deb = self.make_deb(artifacts)
            with self.assertRaises(mod.GateError) as ctx:
                mod.collect_artifacts(artifacts, receipts, "aarch64")
            self.assertIn("receipts directory missing", str(ctx.exception))
            receipts.mkdir()
            with self.assertRaises(mod.GateError) as ctx:
                mod.collect_artifacts(artifacts, receipts, "aarch64")
            self.assertIn("lacks matching PASS D3 receipt", str(ctx.exception))

    def test_valid_d3_artifact_generates_deterministic_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            artifacts = base / "artifacts"
            receipts = base / "receipts"
            artifacts.mkdir()
            deb = self.make_deb(artifacts)
            self.make_d3_receipt(receipts, deb)
            records = mod.collect_artifacts(artifacts, receipts, "aarch64")
            self.assertEqual(len(records), 1)

            out1 = base / "repo1"
            out2 = base / "repo2"
            r1 = mod.generate_repo(records, out1, "aarch64", "stable", "main", 0)
            r2 = mod.generate_repo(records, out2, "aarch64", "stable", "main", 0)
            mod.validate_generated_repo(out1, r1)
            mod.validate_generated_repo(out2, r2)

            self.assertEqual(r1["artifact_count"], 1)
            self.assertEqual(r1["signature"], "NOT_MEASURED")
            self.assertEqual(r1["apt_update"], "NOT_MEASURED")
            self.assertFalse(r1["claim_allowed"])
            self.assertEqual(r1["indices"], r2["indices"])

            packages = out1 / "dists" / "stable" / "main" / "binary-aarch64" / "Packages"
            text = packages.read_text(encoding="utf-8")
            self.assertIn("Package: bash", text)
            self.assertIn("Architecture: aarch64", text)
            self.assertIn("Filename: pool/main/bash_1.0_aarch64.deb", text)
            self.assertIn(f"SHA256: {mod.sha256_file(deb)}", text)

    def test_tampered_artifact_breaks_receipt_binding(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            artifacts = base / "artifacts"
            receipts = base / "receipts"
            artifacts.mkdir()
            deb = self.make_deb(artifacts)
            self.make_d3_receipt(receipts, deb)
            with deb.open("ab") as fh:
                fh.write(b"tamper")
            with self.assertRaises(mod.GateError) as ctx:
                mod.collect_artifacts(artifacts, receipts, "aarch64")
            self.assertIn("lacks matching PASS D3 receipt", str(ctx.exception))

    def test_receipt_metadata_mismatch_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            artifacts = base / "artifacts"
            receipts = base / "receipts"
            artifacts.mkdir()
            deb = self.make_deb(artifacts)
            receipt = self.make_d3_receipt(receipts, deb)
            doc = json.loads(receipt.read_text(encoding="utf-8"))
            doc["package"] = "not-bash"
            receipt.write_text(json.dumps(doc) + "\n", encoding="utf-8")
            with self.assertRaises(mod.GateError) as ctx:
                mod.collect_artifacts(artifacts, receipts, "aarch64")
            self.assertIn("receipt metadata mismatch", str(ctx.exception))

    def test_missing_artifact_set_blocks_d4(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            base = Path(td)
            artifacts = base / "artifacts"
            receipts = base / "receipts"
            artifacts.mkdir()
            receipts.mkdir()
            with self.assertRaises(mod.GateError) as ctx:
                mod.collect_artifacts(artifacts, receipts, "aarch64")
            self.assertIn("no .deb artifacts found", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
