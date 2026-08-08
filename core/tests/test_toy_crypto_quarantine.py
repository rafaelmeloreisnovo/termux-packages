#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "core"

HEADERS = [
    "crypto_ed25519.h",
    "crypto_chacha20.h",
    "crypto_pqc.h",
]

PRODUCTION_TARGETS = [
    "termux-build-core",
    "pkg-real",
    "metrics-producer",
    "pkg-count-freestanding",
    "contract-validate",
    "arch-detect",
]


class ToyCryptoQuarantineTests(unittest.TestCase):
    def _compile_probe(self, header: str, production: bool) -> subprocess.CompletedProcess[str]:
        cmd = ["cc", "-I", str(CORE), "-x", "c", "-fsyntax-only", "-"]
        if production:
            cmd.insert(1, "-DRAF_PRODUCTION_BUILD=1")
        return subprocess.run(
            cmd,
            input=f'#include "{header}"\nint main(void){{return 0;}}\n',
            text=True,
            capture_output=True,
            check=False,
        )

    def test_guard_declares_no_security_or_production_claim(self) -> None:
        text = (CORE / "crypto_toy_guard.h").read_text(encoding="utf-8")
        self.assertIn("RAF_CRYPTO_IMPLEMENTATION_CLASS_SIMULATED_TOY", text)
        self.assertIn("RAF_CRYPTO_SECURITY_CLAIM_ALLOWED 0", text)
        self.assertIn("RAF_CRYPTO_PRODUCTION_ALLOWED 0", text)
        self.assertIn("#ifdef RAF_PRODUCTION_BUILD", text)
        self.assertIn("#error", text)

    def test_legacy_headers_are_explicitly_toy(self) -> None:
        expected = {
            "crypto_ed25519.h": "RAF_ED25519_API_IS_TOY 1",
            "crypto_chacha20.h": "RAF_CHACHA20_API_IS_TOY 1",
            "crypto_pqc.h": "RAF_PQC_API_IS_TOY 1",
        }
        for header, marker in expected.items():
            with self.subTest(header=header):
                text = (CORE / header).read_text(encoding="utf-8")
                self.assertIn('#include "crypto_toy_guard.h"', text)
                self.assertIn(marker, text)
                self.assertIn("SIMULATED_TOY", text)

    def test_test_build_can_still_include_legacy_headers(self) -> None:
        for header in HEADERS:
            with self.subTest(header=header):
                proc = self._compile_probe(header, production=False)
                self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_production_build_rejects_every_toy_header(self) -> None:
        for header in HEADERS:
            with self.subTest(header=header):
                proc = self._compile_probe(header, production=True)
                self.assertNotEqual(proc.returncode, 0)
                self.assertIn("SIMULATED_TOY crypto is forbidden", proc.stderr)

    def test_governed_production_targets_do_not_link_toy_crypto(self) -> None:
        makefile = (CORE / "Makefile").read_text(encoding="utf-8")
        lines = makefile.splitlines()
        target_lines = {line.split(":", 1)[0]: line for line in lines if ":" in line and not line.startswith("\t")}
        for target in PRODUCTION_TARGETS:
            with self.subTest(target=target):
                line = target_lines.get(target)
                self.assertIsNotNone(line, f"missing target {target}")
                self.assertNotIn("crypto_ed25519", line)
                self.assertNotIn("crypto_chacha20", line)
                self.assertNotIn("crypto_pqc", line)

    def test_install_surface_excludes_toy_crypto(self) -> None:
        makefile = (CORE / "Makefile").read_text(encoding="utf-8")
        install = makefile.split("\ninstall:", 1)[1] if "\ninstall:" in makefile else ""
        self.assertTrue(install, "install target missing")
        self.assertNotIn("crypto_ed25519", install)
        self.assertNotIn("crypto_chacha20", install)
        self.assertNotIn("crypto_pqc", install)
        self.assertNotIn("test-security-hardening", install)
        self.assertNotIn("test-integration-e2e", install)


if __name__ == "__main__":
    unittest.main()
