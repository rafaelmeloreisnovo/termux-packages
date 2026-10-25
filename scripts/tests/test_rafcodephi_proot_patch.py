#!/usr/bin/env python3
"""Proot upstream diagnostic-only patch: local self-tests, not Android runtime proof.

Upstream: termux/proot v5.1.107.80 src/cli/cli.c, GPL-2.0.
The fixture reproduces only the two affected diagnostic string literals.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

PATCH = Path(__file__).resolve().parents[2] / "packages/proot/src-cli-cli.c.patch"
OLD = (
    '"It seems that termux-exec is active and is prepending /data/data/com.termux/... to executable paths\\n"\n'
    '"If this is path is not available inside proot, please \\"unset LD_PRELOAD\\"");\n'
)
NEW = (
    '"It seems that termux-exec is active and is prepending an app-private directory to executable paths\\n"\n'
    '"If this path is not available inside proot, please \\"unset LD_PRELOAD\\"");\n'
)

class ProotDiagnosticPatchTests(unittest.TestCase):
    def apply(self, original: str) -> tuple[subprocess.CompletedProcess[str], str]:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src/cli/cli.c"
            source.parent.mkdir(parents=True)
            source.write_text(original, encoding="utf-8")
            cmd = ["patch", "-p1", "--batch", "--forward", "-i", str(PATCH)]
            result = subprocess.run(cmd, cwd=root, text=True, capture_output=True)
            return result, source.read_text(encoding="utf-8")

    def test_patch_is_exact_and_semantic_diagnostic_only(self):
        result, revised = self.apply(OLD)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(revised, NEW)
        self.assertNotIn("/data/data/com.termux/", revised)
        self.assertIn("LD_PRELOAD", revised)

    def test_changed_upstream_diagnostic_fails_closed(self):
        result, unchanged = self.apply('"unexpected upstream diagnostic\\n"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("app-private", unchanged)

    def test_no_broad_code_modification(self):
        patch = PATCH.read_text(encoding="utf-8")
        self.assertIn("--- a/src/cli/cli.c", patch)
        self.assertIn("+++ b/src/cli/cli.c", patch)
        self.assertEqual(sum(line.startswith("-") and not line.startswith("---") for line in patch.splitlines()), 2)
        self.assertEqual(sum(line.startswith("+") and not line.startswith("+++") for line in patch.splitlines()), 2)

if __name__ == "__main__":
    unittest.main(verbosity=2)
