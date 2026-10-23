#!/usr/bin/env python3
"""Rebuild-source .deb falsifiers: corrupt hashes and legacy binary prefix."""
from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/ci"))
import rafcodephi_deb_set_gate as gate


def package(out_dir: Path, payload: bytes, *, historical_doc=False) -> Path:
    build = out_dir / "stage"
    control = build / "DEBIAN"
    control.mkdir(parents=True, exist_ok=True)
    (control / "control").write_text(
        "Package: raf-prefix-fixture\nVersion: 1.0\nArchitecture: all\n"
        "Maintainer: RAFCODEPHI CI <ci@example.invalid>\n"
        "Description: prefix gate negative fixture\n"
    )
    exe = build / "data/data/com.termux.rafacodephi/files/usr/bin/raf-fixture"
    exe.parent.mkdir(parents=True, exist_ok=True)
    exe.write_bytes(payload)
    if historical_doc:
        doc = build / "data/data/com.termux.rafacodephi/files/usr/share/doc/history"
        doc.parent.mkdir(parents=True, exist_ok=True)
        doc.write_bytes(b"Historical /data/data/com.termux/files/usr")
    dest = out_dir / "raf-prefix-fixture_1.0_all.deb"
    subprocess.run(["dpkg-deb", "--root-owner-group", "--build",
                    str(build), str(dest)], capture_output=True, check=True)
    digest = hashlib.sha256(dest.read_bytes()).hexdigest()
    (out_dir / "SHA256SUMS").write_text(f"{digest}  {dest.name}\n")
    return dest


class DebSetGateTests(unittest.TestCase):
    def test_clean_package_set_is_scoped_not_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            folder = Path(root)
            package(folder, b"\x7fELF\x00RAFCODEPHI", historical_doc=True)
            receipt = gate.gate(folder, "arm")
            self.assertEqual(receipt["state"], "PASS_SCOPED")
            self.assertEqual(receipt["device_runtime"], "TOKEN_VAZIO")
            self.assertFalse(receipt["claim_allowed"])

    def test_binary_with_legacy_prefix_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            folder = Path(root)
            package(folder, b"\x7fELF\x00" + b"\x00" * 65530 +
                    b"/data/data/com.termux/files/usr")
            receipt = gate.gate(folder, "arm")
            self.assertEqual(receipt["state"], "BLOCKED")
            self.assertTrue(receipt["legacy_prefix_findings"])
            self.assertEqual(
                receipt["legacy_prefix_findings"][0]["file"],
                "raf-prefix-fixture_1.0_all.deb"
            )

    def test_sha256_mismatch_does_not_fall_back(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            folder = Path(root)
            deb = package(folder, b"\x7fELF\x00VALID")
            deb.write_bytes(deb.read_bytes() + b"bad")
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                gate.gate(folder, "arm")

    def test_missing_or_extra_packages_are_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as root:
            folder = Path(root)
            deb = package(folder, b"\x7fELF\x00VALID")
            (folder / "SHA256SUMS").write_text("")
            with self.assertRaisesRegex(ValueError, "inventory mismatch"):
                gate.indexed_files(folder)
            (folder / "SHA256SUMS").write_text(
                f"{hashlib.sha256(deb.read_bytes()).hexdigest()}  {deb.name}\n")
            (folder / "unlisted.deb").write_bytes(b"dummy")
            with self.assertRaisesRegex(ValueError, "inventory mismatch"):
                gate.indexed_files(folder)


if __name__ == "__main__":
    unittest.main(verbosity=2)
