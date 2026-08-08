#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "scripts" / "validate_pkg_metrics_json.py"
BASELINE = ROOT / "core" / "tests" / "fixtures" / "real_dag_baseline.json"


class ContractAdversarialTests(unittest.TestCase):
    def run_text(self, text: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "candidate.json"
            path.write_text(text, encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(VALIDATOR), str(path)],
                capture_output=True,
                text=True,
                check=False,
            )

    def baseline(self) -> str:
        return BASELINE.read_text(encoding="utf-8")

    def test_valid_baseline_passes(self) -> None:
        proc = self.run_text(self.baseline())
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("STRICT_JSON_GATE=PASS", proc.stdout)

    def test_duplicate_top_level_status_is_rejected(self) -> None:
        text = self.baseline().replace(
            '"status": "REAL",',
            '"status": "REAL",\n  "status": "SIMULATED",',
            1,
        )
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("duplicate key: status", proc.stderr)

    def test_duplicate_provenance_key_is_rejected(self) -> None:
        text = self.baseline().replace(
            '"git_commit": "6f6ac67ac562c5e77daee95f82f82787649c7243",',
            '"git_commit": "6f6ac67ac562c5e77daee95f82f82787649c7243",\n    "git_commit": "shadow",',
            1,
        )
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("duplicate key: git_commit", proc.stderr)

    def test_nested_node_count_cannot_satisfy_top_level_contract(self) -> None:
        text = self.baseline().replace('  "node_count": 3396,\n', '', 1)
        text = text.replace(
            '  "provenance": {',
            '  "shadow": {"node_count": 3396},\n\n  "provenance": {',
            1,
        )
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("node_count must be uint32 at top level", proc.stderr)

    def test_provenance_field_moved_to_top_level_is_rejected(self) -> None:
        text = self.baseline().replace(
            '    "git_commit": "6f6ac67ac562c5e77daee95f82f82787649c7243",\n',
            '',
            1,
        )
        text = text.replace(
            '  "status": "REAL",',
            '  "status": "REAL",\n  "git_commit": "6f6ac67ac562c5e77daee95f82f82787649c7243",',
            1,
        )
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("provenance.git_commit", proc.stderr)

    def test_nonstandard_nan_is_rejected(self) -> None:
        text = self.baseline().replace('"coherence_phi": 0.996195', '"coherence_phi": NaN', 1)
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("non-finite/non-standard JSON number", proc.stderr)

    def test_boolean_cannot_satisfy_integer_field(self) -> None:
        text = self.baseline().replace('"node_count": 3396', '"node_count": true', 1)
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("node_count must be uint32", proc.stderr)

    def test_token_vazio_anywhere_is_rejected(self) -> None:
        text = self.baseline().replace(
            '"repo_base": "."',
            '"repo_base": "TOKEN_VAZIO_hidden"',
            1,
        )
        proc = self.run_text(text)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("TOKEN_VAZIO is forbidden", proc.stderr)


if __name__ == "__main__":
    unittest.main()
