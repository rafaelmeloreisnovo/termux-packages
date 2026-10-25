#!/usr/bin/env python3
"""Isolated deterministic Git fixture tests; no writes to production repository."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import doc_ref_integrity_gate as docgate


class GateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        self.put("README.md", "# Test\n[good](docs/intro.md)\n")
        self.put("docs/intro.md", "# intro\n")
        self.commit()

    def put(self, path, content):
        p = self.root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")

    def commit(self):
        subprocess.run(["git", "add", "-A"], cwd=self.root, check=True, capture_output=True)
        subprocess.run(["git", "-c", "user.name=Test", "-c",
                        "user.email=test@example.invalid", "commit", "-q",
                        "--allow-empty", "-m", "fixture"], cwd=self.root,
                       check=True, capture_output=True)

    def run_gate(self):
        return docgate.audit(self.root, ["README.md"])

    def test_root_inventory_counts_direct_unindexed_dirs(self):
        self.put("free/a.c", "int x;")
        self.commit()
        inv = self.run_gate()["inventory"]
        self.assertEqual(inv["root_files"], 1)
        self.assertEqual(inv["root_dirs"], 2)
        self.assertEqual(inv["missing_direct_index_count"], 2)
        self.assertIn("docs", inv["missing_direct_index_sample"])
        self.assertIn("free", inv["missing_direct_index_sample"])

    def test_valid(self):
        self.assertEqual(self.run_gate()["result"], "PASS_SCOPED")

    def test_missing(self):
        self.put("README.md", "[missing](docs/no.md)\n")
        self.assertEqual(self.run_gate()["findings"][0]["status"], "MISSING_TRACKED_TARGET")

    def test_untracked_not_accepted(self):
        self.put("docs/other.md", "new")
        self.put("README.md", "[no](docs/other.md)")
        self.assertEqual(self.run_gate()["result"], "FAIL_SCOPED")

    def test_outside(self):
        self.put("README.md", "[escape](../../secret.txt)")
        self.assertEqual(self.run_gate()["findings"][0]["status"], "OUTSIDE_REPOSITORY")

    def test_absolute(self):
        self.put("README.md", "[absolute](/etc/passwd)")
        self.assertEqual(self.run_gate()["findings"][0]["status"], "ABSOLUTE_NOT_PORTABLE")

    def test_encoded_parentheses(self):
        self.put("arquivo (1).json", "{}")
        self.put("README.md", "[ok](arquivo%20%281%29.json)")
        self.commit()
        self.assertEqual(self.run_gate()["result"], "PASS_SCOPED")

    def test_fenced_ignored(self):
        self.put("README.md", "~~~md\n[missing](oops.md)\n~~~\n[ok](docs/intro.md)")
        self.assertEqual(self.run_gate()["links_seen"], 1)

    def test_external_and_anchor_not_validated(self):
        self.put("README.md", "[link](https://example.org) [title](#abc)")
        report = self.run_gate()
        self.assertEqual(report["result"], "PASS_SCOPED")
        self.assertEqual(report["unverified_or_skipped"], 2)

    def test_unsafe_scheme(self):
        self.put("README.md", "[bad](javascript:alert)")
        self.assertEqual(self.run_gate()["findings"][0]["status"], "UNSAFE_SCHEME")

    def test_symlink_escape(self):
        (self.root / "docs" / "jump").symlink_to("/etc")
        self.put("README.md", "[bad](docs/jump/passwd)")
        self.assertEqual(self.run_gate()["findings"][0]["status"], "SYMLINK_ESCAPE")

    def test_unicode_path(self):
        self.put("pasta/coração.md", "# ok")
        self.put("README.md", "[lindo](pasta/cora%C3%A7%C3%A3o.md)")
        self.commit()
        self.assertEqual(self.run_gate()["result"], "PASS_SCOPED")

    def test_missing_source(self):
        res = docgate.audit(self.root, ["absent.md"])
        self.assertEqual(res["findings"][0]["status"], "DOC_NOT_TRACKED_OR_NOT_MD")

    def test_directory_link(self):
        self.put("README.md", "[folder](docs/)")
        self.assertEqual(self.run_gate()["result"], "PASS_SCOPED")


if __name__ == "__main__":
    unittest.main(verbosity=2)
