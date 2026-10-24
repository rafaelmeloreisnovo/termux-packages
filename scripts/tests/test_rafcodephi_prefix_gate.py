#!/usr/bin/env python3
"""Independent negative falsifiers for the host gate and freestanding kernel."""
from __future__ import annotations

import ctypes
import importlib.util
import json
import random
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
GATE_PATH = ROOT / "scripts/ci/rafcodephi_bootstrap_prefix_gate.py"
CORE_PATH = ROOT / "scripts/rafcodephi-freestanding/rafcodephi_prefix_gate_core.c"
spec = importlib.util.spec_from_file_location("raf_prefix_gate", GATE_PATH)
gate = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(gate)


def fixture(path: Path, *, unsafe: bytes | None = None,
            unsafe_entry: str = "bin/apt", duplicate: bool = False) -> None:
    profile = {
        "profile": "real-pkg", "package_layer": "real-pkg",
        "package_name": gate.PACKAGE, "prefix": gate.PREFIX, "arch": "arm",
        "legacy_prefix_forbidden": True, "claim_allowed": False,
        "device_validation": "TOKEN_VAZIO",
        "release_allowed": False,
    }
    entries = {
        "BOOTSTRAP_INFO": (
            b"RAFCODEPHI_BOOTSTRAP_PROFILE=real-pkg\n"
            b"RAFCODEPHI_PACKAGE_LAYER=real-pkg\n"
            b"RAFCODEPHI_CLAIM_ALLOWED=0\n"
        ),
        "BOOTSTRAP_PROFILE.json": json.dumps(profile).encode(),
        "SYMLINKS.txt": b"sh\xe2\x86\x90bin/bash\n",
        "bin/apt": b"\x7fELF\x00" + b"RAFCODEPHI",
        "bin/apt-get": b"\x7fELF\x00",
        "bin/dpkg": b"\x7fELF\x00",
        "bin/pkg": b"#!/system/bin/sh\n",
        "bin/bash": b"\x7fELF\x00",
        "share/doc/legacy-history.txt": b"/data/data/com.termux/",
    }
    if unsafe is not None:
        entries[unsafe_entry] = unsafe
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as out:
        for name, payload in entries.items():
            out.writestr(name, payload)
        if duplicate:
            out.writestr("./bin/apt", b"\x7fELF\x00")


class PrefixGateTests(unittest.TestCase):
    def test_clean_scoped_pass_not_device_proof(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            sample = Path(folder) / "good.zip"
            fixture(sample)
            receipt = gate.gate(sample, "arm")
            self.assertEqual(receipt["state"], "PASS_SCOPED")
            self.assertFalse(receipt["claim_allowed"])
            self.assertEqual(receipt["device_runtime"], "TOKEN_VAZIO")

    def test_legacy_elf_is_blocked_across_chunk_boundary(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            sample = Path(folder) / "bad.zip"
            payload = b"\x7fELF\x00" + b"A" * (gate.CHUNK - 5 - 11)
            payload += b"/data/data/com.termux/files/usr"
            fixture(sample, unsafe=payload)
            report = gate.gate(sample, "arm")
            self.assertEqual(report["state"], "BLOCKED")
            self.assertIn("bin/apt", report["legacy_prefix_entries"])
            self.assertFalse(report["release_allowed"])

    def test_legacy_apt_text_is_blocked_but_history_is_not(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            sample = Path(folder) / "bad-config.zip"
            fixture(sample, unsafe=b"source=/data/data/com.termux/files/usr\n",
                    unsafe_entry="etc/apt/sources.list")
            report = gate.gate(sample, "arm")
            self.assertIn("etc/apt/sources.list", report["legacy_prefix_entries"])

    def test_duplicate_normalized_zip_member_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            sample = Path(folder) / "dup.zip"
            fixture(sample, duplicate=True)
            with self.assertRaisesRegex(ValueError, "duplicate"):
                gate.gate(sample, "arm")

    def test_wrong_arch_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            sample = Path(folder) / "wrong.zip"
            fixture(sample)
            with self.assertRaisesRegex(ValueError, "arch"):
                gate.gate(sample, "aarch64")

    def test_freestanding_kernel_no_undefined_imports_and_negative_cases(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            lib = Path(folder) / "libprefix_sentinel.so"
            subprocess.run([
                "cc", "-std=c17", "-O2", "-Wall", "-Wextra", "-Werror",
                "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
                "-nostdlib", "-fPIC", "-shared", "-Wl,-z,noexecstack",
                str(CORE_PATH), "-o", str(lib)
            ], check=True, capture_output=True)
            undefined = subprocess.run(["nm", "-u", str(lib)],
                                       check=True, capture_output=True, text=True)
            self.assertFalse(undefined.stdout.strip(), undefined.stdout)
            ffi = ctypes.CDLL(str(lib))
            fn = ffi.rafcodephi_legacy_prefix_in_span
            fn.argtypes = (ctypes.c_void_p, ctypes.c_size_t)
            fn.restype = ctypes.c_uint
            for n in (0, 1, len(gate.LEGACY), ctypes.c_size_t(-1).value):
                self.assertEqual(fn(None, n), 2, ("NULL", n))
            for buf, expected in (
                (b"/data/data/com.termux/files/usr", 1),
                (b"/data/data/com.termux.rafacodephi/files/usr", 0),
                (b"start\x00/data/data/com.termux/relative", 1),
                (b"", 0),
            ):
                value = ctypes.create_string_buffer(buf)
                self.assertEqual(fn(value, len(buf)), expected, buf)
            # Compare with an independent bytes search, including all prefix
            # truncations, offsets, and partial matches at the end of a span.
            rng = random.Random(144000)
            cases = [gate.LEGACY[:n] for n in range(len(gate.LEGACY) + 1)]
            cases += [b"x" * n + gate.LEGACY + b"y" * (64 - n)
                      for n in range(65)]
            for _ in range(512):
                buf = bytes(rng.randrange(256) for _ in range(rng.randrange(128)))
                if rng.randrange(2):
                    pos = rng.randrange(len(buf) + 1)
                    buf = buf[:pos] + gate.LEGACY + buf[pos:]
                cases.append(buf)
            for buf in cases:
                value = ctypes.create_string_buffer(buf)
                for n in (0, len(buf) // 2, len(buf)):
                    self.assertEqual(fn(value, n), int(gate.LEGACY in buf[:n]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
