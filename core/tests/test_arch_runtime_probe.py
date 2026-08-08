#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import shutil
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "arch_runtime_probe.py"

spec = importlib.util.spec_from_file_location("arch_runtime_probe", SCRIPT)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class ArchRuntimeProbeTests(unittest.TestCase):
    def test_report_scope_never_promotes_device_or_cross_arch(self) -> None:
        report = mod.build_report()
        self.assertEqual(report["schema"], "arch_runtime_probe/1.0.0")
        self.assertEqual(report["status"], "OBSERVED_LIMITED")
        self.assertFalse(report["claim_allowed"])
        self.assertFalse(report["scope"]["nominal_arch_matrix_is_runtime_evidence"])
        self.assertFalse(report["scope"]["physical_device_verified"])
        self.assertFalse(report["scope"]["cross_arch_execution_verified"])
        self.assertEqual(report["device"]["state"], "TOKEN_VAZIO")

    def test_runtime_identity_comes_from_platform(self) -> None:
        report = mod.build_report()
        raw = report["identity"]["machine_raw"]
        self.assertIn(raw["state"], {"OBSERVED", "TOKEN_VAZIO"})
        if raw["state"] == "OBSERVED":
            self.assertEqual(raw["source"], "platform.machine()")
            self.assertTrue(raw["value"])

    def test_page_size_is_observed_or_explicit_gap(self) -> None:
        item = mod.probe_page_size()
        self.assertIn(item["state"], {"OBSERVED", "TOKEN_VAZIO"})
        if item["state"] == "OBSERVED":
            self.assertGreater(item["value"], 0)
            self.assertEqual(item["source"], "os.sysconf(SC_PAGESIZE)")
        else:
            self.assertIn("reason", item)

    def test_cache_line_has_no_typical_fallback(self) -> None:
        item = mod.probe_cache_line()
        self.assertIn(item["state"], {"OBSERVED", "TOKEN_VAZIO"})
        if item["state"] == "OBSERVED":
            self.assertGreater(item["value"], 0)
            self.assertTrue(str(item["source"]).startswith("/sys/"))
        else:
            self.assertIsNone(item["value"])
            self.assertIn("reason", item)

    def test_simd_claims_are_derived_only_from_observed_flags(self) -> None:
        synthetic = {"sse2", "avx2", "asimd", "sve"}
        item = mod.probe_simd(synthetic, "/tmp/cpuinfo-fixture")
        self.assertEqual(item["state"], "OBSERVED")
        value = item["value"]
        self.assertTrue(value["sse2"])
        self.assertTrue(value["avx2"])
        self.assertTrue(value["neon"])
        self.assertTrue(value["sve"])
        self.assertFalse(value["avx512f"])
        self.assertFalse(value["altivec"])

    def test_missing_cpuinfo_becomes_token_vazio(self) -> None:
        item = mod.probe_simd(set(), None)
        self.assertEqual(item["state"], "TOKEN_VAZIO")
        self.assertIsNone(item["value"])

    def test_emulator_available_only_when_executable_resolves(self) -> None:
        item = mod.probe_emulators()
        self.assertEqual(item["state"], "OBSERVED")
        for arch, record in item["value"].items():
            with self.subTest(arch=arch):
                expected = shutil.which(record["executable"])
                self.assertEqual(record["available"], expected is not None)
                self.assertEqual(record["path"], expected)

    def test_normalization_does_not_invent_unknown_arch(self) -> None:
        self.assertEqual(mod.normalize_machine("mystery-cpu", "Linux"), "unknown")
        self.assertEqual(mod.normalize_machine("aarch64", "Linux"), "arm64")
        self.assertEqual(mod.normalize_machine("arm64", "Darwin"), "arm64_darwin")


if __name__ == "__main__":
    unittest.main()
