#!/usr/bin/env python3
"""Positive and adversarial tests for separate source-lot provenance."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ci"))
import rafcodephi_batch_manifest as lot

COMMIT = "a" * 40


def hash_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(root: Path, arch: str) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    debdir = root / "debs"
    debdir.mkdir()
    package = debdir / f"sample_1.0_{arch}.deb"
    package.write_bytes(b"fixture-data-only-not-a-real-deb:" + arch.encode())
    (debdir / "SHA256SUMS").write_text(
        f"{hash_of(package)}  ./{package.name}\n", encoding="utf-8"
    )
    with zipfile.ZipFile(root / "bootstrap.zip", "w") as zf:
        zf.writestr("BOOTSTRAP_PROFILE.json", json.dumps({"profile": "real-pkg"}))
        zf.writestr("BOOTSTRAP_INFO", "CLAIM_ALLOWED=0\n")
    for name in ("prefix", "deb"):
        (root / f"{name}-gate.json").write_text(json.dumps({
            "state": "PASS_SCOPED", "arch": arch,
            "claim_allowed": False, "device_runtime": "TOKEN_VAZIO",
        }) + "\n")
    rows = {
        "schema": "rafcodephi.real-bootstrap-sourcebuild/v1",
        "package_name": "com.termux.rafacodephi",
        "prefix": "/data/data/com.termux.rafacodephi/files/usr",
        "bridge_allowed": "false",
        "legacy_prefix_allowed": "false",
        "device_runtime_proof": "TOKEN_VAZIO",
        f"prefix_gate_state_{arch}": "PASS_SCOPED",
        f"prefix_gate_receipt_sha256_{arch}": hash_of(root / "prefix-gate.json"),
        f"deb_gate_state_{arch}": "PASS_SCOPED",
        f"deb_gate_receipt_sha256_{arch}": hash_of(root / "deb-gate.json"),
        f"deb_count_{arch}": "1",
        f"deb_set_sha256_{arch}": hash_of(debdir / "SHA256SUMS"),
        f"bytes_{arch}": str((root / "bootstrap.zip").stat().st_size),
    }
    (root / "manifest.txt").write_text(
        "".join(f"{k}={v}\n" for k, v in sorted(rows.items())),
        encoding="utf-8",
    )
    return root


class SourceLotTests(unittest.TestCase):
    def test_independent_lots_and_aggregate(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            arm = fixture(base / "arm", "arm")
            aarch64 = fixture(base / "aarch64", "aarch64")
            a = lot.audit(arm, "arm", COMMIT)
            self.assertEqual(a["state"], "PASS_SCOPED")
            self.assertFalse(a["claim_allowed"])
            r = lot.aggregate(arm, aarch64, COMMIT)
            self.assertEqual(r["state"], "PASS_SCOPED")
            self.assertEqual(set(r["architectures"]), {"arm", "aarch64"})
            self.assertEqual(r["publish"], "BLOCKED")
            self.assertFalse(r["claim_allowed"])

    def test_hash_tamper_blocks(self):
        with tempfile.TemporaryDirectory() as temp:
            root = fixture(Path(temp) / "arm", "arm")
            with (root / "debs" / "sample_1.0_arm.deb").open("ab") as f:
                f.write(b"TAMPER")
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                lot.audit(root, "arm", COMMIT)

    def test_gate_falsifier_blocks(self):
        with tempfile.TemporaryDirectory() as temp:
            root = fixture(Path(temp) / "arm", "arm")
            p = root / "prefix-gate.json"
            data = json.loads(p.read_text())
            data["state"] = "BLOCKED"
            p.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "gate mismatch"):
                lot.audit(root, "arm", COMMIT)

    def test_cross_architecture_and_invalid_sha_fail(self):
        with tempfile.TemporaryDirectory() as temp:
            root = fixture(Path(temp) / "arm", "arm")
            with self.assertRaisesRegex(ValueError, "gate mismatch"):
                lot.audit(root, "aarch64", COMMIT)
            with self.assertRaisesRegex(ValueError, "invalid architecture or source SHA"):
                lot.audit(root, "arm", "unknown")

    def test_duplicate_sha_alias_fail(self):
        with tempfile.TemporaryDirectory() as temp:
            root = fixture(Path(temp) / "arm", "arm")
            p = root / "debs" / "sample_1.0_arm.deb"
            digest = hash_of(p)
            (root / "debs" / "SHA256SUMS").write_text(
                f"{digest}  {p.name}\n{digest}  ./{p.name}\n"
            )
            with self.assertRaisesRegex(ValueError, "duplicate SHA256SUMS"):
                lot.audit(root, "arm", COMMIT)


if __name__ == "__main__":
    unittest.main(verbosity=2)
