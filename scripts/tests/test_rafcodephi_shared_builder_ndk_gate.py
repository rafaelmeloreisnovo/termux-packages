#!/usr/bin/env python3
"""Offline behavioral gate for the shared RAFCODEPHI Docker bootstrap wrapper.

No actual Docker, network, downloaded NDK, Android SDK or source build is run.
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "scripts/run-rafcodephi-bootstrap-docker.sh"
NDK_PATH = "/home/builder/.termux-build/android-ndk-r29"


class WrapperNDKGate(unittest.TestCase):
    def fixture(self, *, preflight_failure=False, missing_helper=False):
        temp = tempfile.TemporaryDirectory(prefix="raf-ndk-gate-")
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        scripts = root / "scripts"
        (scripts / "ci").mkdir(parents=True)
        (root / "fakebin").mkdir()
        shutil.copyfile(WRAPPER, scripts / WRAPPER.name)
        if not missing_helper:
            (scripts / "ci/rafcodephi_prepare_ndk29.sh").write_text(
                "#!/bin/bash\nexit 0\n", encoding="utf-8"
            )
        runner = scripts / "run-docker.sh"
        runner.write_text(
            "#!/bin/bash\n"
            "printf '%s\\n' \"$*\" >> \"$TRACE_FILE\"\n"
            "if [[ \"$1\" == bash && \"$2\" == scripts/ci/rafcodephi_prepare_ndk29.sh"
            " && \"${PREFLIGHT_FAIL:-0}\" == 1 ]]; then exit 47; fi\n"
            "exit 0\n", encoding="utf-8"
        )
        runner.chmod(0o755)
        fake = root / "fakebin/docker"
        fake.write_text(
            "#!/bin/bash\n"
            "case \"$1\" in\n"
            "  exec) exit 0 ;;\n"
            "  cp) mkdir -p \"$3\"; printf 'synthetic\\n' >"
            " \"$3/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt\"; exit 0 ;;\n"
            "esac\n"
            "exit 22\n", encoding="utf-8"
        )
        fake.chmod(0o755)
        trace = root / "trace.txt"
        env = dict(os.environ)
        env["PATH"] = str(root / "fakebin") + os.pathsep + env.get("PATH", "")
        env["TRACE_FILE"] = str(trace)
        env["PREFLIGHT_FAIL"] = "1" if preflight_failure else "0"
        env.pop("TERMUX_DOCKER_USE_SUDO", None)
        return root, env, trace

    def invoke(self, root, env):
        return subprocess.run(
            ["bash", str(root / "scripts/run-rafcodephi-bootstrap-docker.sh"),
             "bash", "scripts/build-rafcodephi-real-bootstrap.sh",
             "--architectures", "arm,aarch64"],
            cwd=root, env=env, text=True, capture_output=True, timeout=20,
            check=False,
        )

    def test_verified_preflight_precedes_cleanup_and_build(self):
        root, env, trace = self.fixture()
        result = self.invoke(root, env)
        self.assertEqual(0, result.returncode, result.stderr)
        calls = trace.read_text(encoding="utf-8").splitlines()
        self.assertEqual(3, len(calls), calls)
        self.assertEqual(
            "bash scripts/ci/rafcodephi_prepare_ndk29.sh", calls[0]
        )
        self.assertTrue(calls[1].startswith("sh -c "), calls)
        self.assertIn("env NDK=" + NDK_PATH, calls[2])
        self.assertIn("scripts/build-rafcodephi-real-bootstrap.sh", calls[2])
        self.assertIn("RAFCODEPHI_BOOTSTRAP_DOCKER=PASS", result.stdout)

    def test_rejected_preflight_never_builds_or_erases_prior_evidence(self):
        root, env, trace = self.fixture(preflight_failure=True)
        prior = root / "artifacts/rafcodephi-bootstrap/prior-receipt.txt"
        prior.parent.mkdir(parents=True)
        prior.write_text("keep historical receipt\n", encoding="utf-8")
        result = self.invoke(root, env)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("builder_preflight_failed", result.stderr)
        self.assertEqual(
            ["bash scripts/ci/rafcodephi_prepare_ndk29.sh"],
            trace.read_text(encoding="utf-8").splitlines()
        )
        self.assertEqual("keep historical receipt\n", prior.read_text())

    def test_missing_provisioner_is_fail_closed(self):
        root, env, trace = self.fixture(missing_helper=True)
        result = self.invoke(root, env)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("missing_pinned_provisioner", result.stderr)
        self.assertFalse(trace.exists())

    def test_bash_syntax(self):
        result = subprocess.run(
            ["bash", "-n", str(WRAPPER)],
            capture_output=True, text=True, check=False
        )
        self.assertEqual(0, result.returncode, result.stderr)


if __name__ == "__main__":
    unittest.main()
