#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "core" / "audit_reality_v2.py"
REGISTRY = ROOT / "core" / "reality_registry.v2.json"

spec = importlib.util.spec_from_file_location("audit_reality_v2", SCRIPT)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class RealityAuditV2Tests(unittest.TestCase):
    def test_registry_contract_is_valid(self) -> None:
        registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
        self.assertEqual(mod.validate_registry(registry), [])
        self.assertTrue(all(item["claim_allowed"] is False for item in registry["modules"]))

    def test_unregistered_module_becomes_token_vazio(self) -> None:
        registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as td:
            core = Path(td) / "core"
            core.mkdir()
            (core / "unknown_module.c").write_text("int f(void){return 1;}\n", encoding="utf-8")
            report = mod.build_report(core, registry)
            item = report["modules"][0]
            self.assertEqual(item["classification"], "TOKEN_VAZIO")
            self.assertEqual(item["registry_state"], "UNREGISTERED")
            self.assertFalse(item["claim_allowed"])
            self.assertTrue(all(v == "TOKEN_VAZIO" for v in item["evidence"].values()))

    def test_heuristics_do_not_promote_unknown_to_real(self) -> None:
        registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as td:
            core = Path(td) / "core"
            core.mkdir()
            (core / "io_heavy.c").write_text(
                "int f(void){ open(0,0); read(0,0,0); write(1,0,0); return 0; }\n",
                encoding="utf-8",
            )
            report = mod.build_report(core, registry)
            item = report["modules"][0]
            self.assertGreaterEqual(item["signals"]["io"], 3)
            self.assertEqual(item["classification"], "TOKEN_VAZIO")

    def test_simulated_crypto_is_explicit_and_security_fails(self) -> None:
        registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
        crypto = {item["file"]: item for item in registry["modules"]}
        for name in ("crypto_ed25519.c", "crypto_chacha20.c", "crypto_pqc.c"):
            with self.subTest(name=name):
                self.assertEqual(crypto[name]["classification"], "SIMULATED")
                self.assertEqual(crypto[name]["priority"], "P0")
                self.assertEqual(crypto[name]["evidence"]["security"], "FAIL")
                self.assertFalse(crypto[name]["claim_allowed"])

    def test_claim_true_without_promotion_evidence_is_rejected(self) -> None:
        registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
        broken = json.loads(json.dumps(registry))
        broken["modules"][0]["claim_allowed"] = True
        errors = mod.validate_registry(broken)
        self.assertTrue(any("promotion axes not PASS" in error for error in errors))

    def test_strict_gate_blocks_p0_unresolved(self) -> None:
        registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as td:
            td_path = Path(td)
            core = td_path / "core"
            core.mkdir()
            source = ROOT / "core" / "crypto_pqc.c"
            (core / "crypto_pqc.c").write_text(source.read_text(encoding="utf-8"), encoding="utf-8")
            reg_path = td_path / "registry.json"
            reg_path.write_text(json.dumps(registry), encoding="utf-8")
            out_path = td_path / "report.json"
            proc = subprocess.run(
                [sys.executable, str(SCRIPT), "--core-dir", str(core), "--registry", str(reg_path), "--out", str(out_path), "--strict"],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(proc.returncode, 1)
            self.assertIn("STRICT_GATE=BLOCKED", proc.stderr)
            report = json.loads(out_path.read_text(encoding="utf-8"))
            self.assertFalse(report["gate"]["strict_pass"])


if __name__ == "__main__":
    unittest.main()
