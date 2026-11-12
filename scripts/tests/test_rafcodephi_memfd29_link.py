#!/usr/bin/env python3
"""Host linker falsifiers: only source ABI compatibility, never device E2E."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
LINKER = ROOT / "scripts/ci/rafcodephi_memfd29_link.sh"
C_SOURCE = r'''int memfd_create(const char *, unsigned int);
int raf_memfd_fixture(void) { return memfd_create("rafcodephi", 0); }
'''


@unittest.skipUnless(shutil.which("cc") and shutil.which("readelf"), "host C toolchain required")
class CompatibilityLinkTests(unittest.TestCase):
    def build(self, *, arch="aarch64", api="29", package="com.termux.rafacodephi", compiler="cc"):
        with tempfile.TemporaryDirectory(prefix="raf-memfd29-test-") as temp:
            tmp = Path(temp)
            src = tmp / "fixture.c"
            lib = tmp / "fixture.so"
            src.write_text(C_SOURCE, encoding="utf-8")
            env = os.environ.copy()
            env.update(TERMUX_SCRIPTDIR=str(ROOT), TERMUX_PKG_TMPDIR=str(tmp),
                       TERMUX_ARCH=arch, TERMUX_PKG_API_LEVEL=api,
                       TERMUX_APP__PACKAGE_NAME=package, TERMUX_PKG_NAME="test-fixture",
                       CC=compiler)
            script = (f'set -euo pipefail; source "{LINKER}"; '
                      'LDFLAGS="-Wl,-z,relro"; '
                      'rafcodephi_apply_memfd29_link; '
                      'cc -shared -fPIC "' + str(src) + '" -o "' + str(lib) + '" '
                      '-DRAFCODEPHI_MEMFD29_HOST_TEST=1 $LDFLAGS')
            if compiler == "cc":
                cc = tmp / "cc"
                cc.write_text("#!/usr/bin/env bash\nexec /usr/bin/cc -DRAFCODEPHI_MEMFD29_HOST_TEST=1 \"$@\"\n")
                cc.chmod(0o755)
                env["CC"] = str(cc)
            proc = subprocess.run(["bash", "-c", script], capture_output=True,
                                  text=True, env=env, check=False)
            if proc.returncode:
                return proc, ""
            symbols = subprocess.run(["readelf", "--dyn-syms", "--wide", str(lib)],
                                     check=True, capture_output=True, text=True).stdout
            return proc, symbols

    def test_api29_aarch64_has_no_memfd_undefined(self):
        proc, symbols = self.build()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("RAFCODEPHI_MEMFD29=SOURCE_LINK_ADAPTER", proc.stdout)
        self.assertFalse(any(" UND " in line and re.search(r"\bmemfd_create\b", line)
                             for line in symbols.splitlines()), symbols)
        self.assertIn("__wrap_memfd_create", symbols)

    def test_other_android_api_remains_unchanged(self):
        proc, symbols = self.build(api="30")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertNotIn("SOURCE_LINK_ADAPTER", proc.stdout)
        self.assertTrue(any(" UND " in line and "memfd_create" in line
                            for line in symbols.splitlines()))

    def test_arm32_remains_unchanged(self):
        proc, symbols = self.build(arch="arm")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertNotIn("SOURCE_LINK_ADAPTER", proc.stdout)

    def test_other_package_remains_unchanged(self):
        proc, symbols = self.build(package="com.termux")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertNotIn("SOURCE_LINK_ADAPTER", proc.stdout)

    def test_compiler_failure_is_not_silently_accepted(self):
        proc, _ = self.build(compiler="/bin/false")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("RAFCODEPHI_MEMFD29=BLOCKED", proc.stderr)


if __name__ == "__main__":
    unittest.main()
