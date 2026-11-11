#!/usr/bin/env python3
"""P0 source bootstrap gate: behavioral falsifiers without Android/emulator."""
from __future__ import annotations

import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/ci/rafcodephi_android_api_gate.py"
spec = importlib.util.spec_from_file_location("raf_api29_source_gate", SCRIPT)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

NOTE_29 = (
    "Displaying notes found in: .note.android.ident\n"
    "  Owner                Data size Description\n"
    "  Android              0x00000084 NT_VERSION (version)\n"
    "   description data: 1d 00 00 00 72 32 39 00\n"
)
NOTE_30 = NOTE_29.replace("1d 00 00 00", "1e 00 00 00")
SYMS_SAFE = "  1: 00000000 0 FUNC GLOBAL DEFAULT UND read@LIBC (2)\n"
SYMS_UNSAFE = (
    "  18: 00000000 0 FUNC GLOBAL DEFAULT UND memfd_create@LIBC_R (7)\n"
)


def fake_elf(arch: str) -> bytes:
    data = bytearray(64)
    data[:4] = b"\x7fELF"
    data[4] = module.ABI_ELF[arch][0]
    data[5] = 1
    data[18:20] = module.ABI_ELF[arch][1].to_bytes(2, "little")
    return bytes(data)


def simulate_readelf(note: str = NOTE_29, symbols: str = SYMS_SAFE):
    def run(argv, **kwargs):
        data = note if "-n" in argv else symbols
        return SimpleNamespace(returncode=0, stdout=data, stderr="")
    return run


class SourceBootstrapApiTests(unittest.TestCase):
    def test_real_multiline_android30_note_parses(self):
        self.assertEqual(module.parse_android_api(NOTE_30), 30)

    def test_multiline_android29_note_parses(self):
        self.assertEqual(module.parse_android_api(NOTE_29), 29)

    def test_missing_note_is_unknown_not_zero(self):
        self.assertIsNone(module.parse_android_api("no Android note"))

    def test_api30_fails_on_android10(self):
        with patch.object(module.subprocess, "run",
                          side_effect=simulate_readelf(NOTE_30)):
            item = module.check_elf(fake_elf("arm"), "bin/bash", "arm", 29, "readelf")
        self.assertIn("ANDROID_MIN_API_EXCEEDS_TARGET", item["reasons"])

    def test_api29_without_forbidden_import_passes_static_only(self):
        with patch.object(module.subprocess, "run",
                          side_effect=simulate_readelf(NOTE_29)):
            item = module.check_elf(fake_elf("arm"), "bin/bash", "arm", 29, "readelf")
        self.assertEqual(item["state"], "PASS_SCOPED_STATIC")

    def test_api29_with_unresolved_memfd_is_blocked(self):
        with patch.object(module.subprocess, "run",
                          side_effect=simulate_readelf(NOTE_29, SYMS_UNSAFE)):
            item = module.check_elf(fake_elf("arm"), "bin/bash", "arm", 29, "readelf")
        self.assertIn("UNRESOLVED_MEMFD_CREATE", item["reasons"])

    def test_arm64_elf_rejected_by_arm32_gate(self):
        item = module.check_elf(fake_elf("aarch64"), "bin/bash", "arm", 29, "readelf")
        self.assertIn("ABI_OR_ENDIANNESS_MISMATCH", item["reasons"])

    def test_incomplete_zip_must_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bootstrap-arm.zip"
            with zipfile.ZipFile(path, "w") as zf:
                zf.writestr("bin/bash", fake_elf("arm"))
            with patch.object(module, "check_elf", return_value={
                "path": "bin/bash", "state": "PASS_SCOPED_STATIC", "api": 29,
                "reasons": [],
            }):
                doc = module.verify(path, "arm", 29, "readelf")
        self.assertEqual(doc["state"], "BLOCKED")
        self.assertTrue(any(x["reasons"] == ["REQUIRED_MISSING"] for x in doc["issues"]))

    def test_complete_fixture_can_pass_scope_without_device_claim(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bootstrap-arm.zip"
            with zipfile.ZipFile(path, "w") as zf:
                for name in module.REQUIRED_ELF:
                    zf.writestr(name, fake_elf("arm"))
                for name, line in module.REQUIRED_SHEBANG.items():
                    zf.writestr(name, line + "\n")
                zf.writestr("SYMLINKS.txt", "dash←./bin/sh\n")
            with patch.object(module, "check_elf", side_effect=lambda d, p, a, level, elf:
                              {"path": p, "state": "PASS_SCOPED_STATIC",
                               "api": 29, "reasons": []}):
                doc = module.verify(path, "arm", 29, "readelf")
        self.assertEqual(doc["state"], "PASS_SCOPED_STATIC")
        self.assertFalse(doc["claim_allowed"])
        self.assertEqual(doc["device_execution"], "TOKEN_VAZIO")

    def test_bad_pkg_shebang_is_blocked(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bootstrap-arm.zip"
            with zipfile.ZipFile(path, "w") as zf:
                for name in module.REQUIRED_ELF:
                    zf.writestr(name, fake_elf("arm"))
                for name, line in module.REQUIRED_SHEBANG.items():
                    zf.writestr(name, (
                        "#!/data/data/com.termux/files/usr/bin/bash\n"
                        if name == "bin/pkg" else line + "\n"
                    ))
                zf.writestr("SYMLINKS.txt", "dash←./bin/sh\n")
            with patch.object(module, "check_elf", side_effect=lambda d, p, a, level, elf:
                              {"path": p, "state": "PASS_SCOPED_STATIC",
                               "api": 29, "reasons": []}):
                doc = module.verify(path, "arm", 29, "readelf")
        self.assertEqual(doc["state"], "BLOCKED")
        self.assertTrue(any(x["reasons"] == ["SHEBANG_MISMATCH"] for x in doc["issues"]))


if __name__ == "__main__":
    unittest.main()
