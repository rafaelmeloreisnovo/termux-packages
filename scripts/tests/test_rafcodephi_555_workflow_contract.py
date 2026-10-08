#!/usr/bin/env python3
"""No-build tests for the RAFCODEPHI 555 manual dispatch safety boundary."""
from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/555-artifact-manifold.yml"
GOOD_SHA = "7" * 40


def must(condition: bool, label: str) -> None:
    if not condition:
        raise AssertionError("555_WORKFLOW_BLOCKED: " + label)


def verify_case(script: str, tmp: Path, name: str, *,
                origin: str, operation: str, run_id: str,
                artifact_name: str, expected_sha: str,
                event: str, path: str, head_repo: str,
                head_branch: str, expect_pass: bool) -> None:
    metadata = {
        "repository": {"full_name": "rafaelmeloreisnovo/termux-packages"},
        "head_repository": {"full_name": head_repo},
        "head_branch": head_branch,
        "head_sha": GOOD_SHA,
        "event": event,
        "path": path,
        "conclusion": "failure",
    }
    source = tmp / "mock-run.json"
    source.write_text(json.dumps(metadata), encoding="utf-8")
    output = tmp / "job-output"
    output.write_text("", encoding="utf-8")
    env = dict(os.environ)
    env.update({
        "ORIGIN": origin,
        "OPERATION": operation,
        "RUN_ID": run_id,
        "ARTIFACT_NAME": artifact_name,
        "EXPECTED_SHA": expected_sha,
        "GITHUB_REPOSITORY": "rafaelmeloreisnovo/termux-packages",
        "GITHUB_OUTPUT": str(output),
        "GH_META_FILE": str(source),
        "GH_TOKEN": "synthetic-test-only",
        "PATH": str(tmp) + os.pathsep + env["PATH"],
    })
    result = subprocess.run(["bash", "-e"], input=script, capture_output=True,
                            text=True, env=env, check=False)
    must((result.returncode == 0) == expect_pass,
         f"{name}: unexpected exit {result.returncode} stderr={result.stderr[-250:]!r}")
    if expect_pass and origin == "artifact-run":
        must(f"producer_sha={GOOD_SHA}" in output.read_text(encoding="utf-8"),
             f"{name}: producer_sha not bound")


def main() -> int:
    raw = WORKFLOW.read_text(encoding="utf-8")
    must(not re.search(r"\$\{\{\s+['\"]?\$[A-Za-z_]", raw),
         "BASH_VARIABLE_INSIDE_ACTIONS_INTERPOLATION")
    doc = yaml.safe_load(raw)
    inputs = doc["on" if "on" in doc else True]["workflow_dispatch"]["inputs"]
    must("expected_producer_sha" in inputs, "EXPECTED_SHA_INPUT_MISSING")
    steps = doc["jobs"]["compose"]["steps"]
    matches = [step for step in steps if step.get("name") == "Verify options and producer authority"]
    must(len(matches) == 1, "GUARD_STEP_AMBIGUOUS")
    script = matches[0]["run"]
    must("rafcodephi-publish-dev-apt.yml" in script, "SIGNED_APT_WORKFLOW_NOT_ALLOWED")
    must("head_repository.full_name == $repo" in script, "SAME_REPO_BOUNDARY_MISSING")
    must('[[ "$EXPECTED_SHA" == "$producer_sha" ]]' in script, "PR_HEAD_SHA_FALSIFIER_MISSING")
    must('if [[ "$ORIGIN" == "source-contract" ]]; then' in script,
         "SOURCE_CONTRACT_BASH_GUARD_MISSING")
    must('if [[ "$ORIGIN" == "artifact-run" ]]' in doc["jobs"]["compose"]["steps"][3]["run"],
         "ARTIFACT_INPUT_BASH_GUARD_MISSING")
    must(subprocess.run(["bash", "-n"], input=script, text=True, capture_output=True,
                        check=False).returncode == 0, "BASH_PARSE_FAILED")

    with tempfile.TemporaryDirectory(prefix="rafcodephi-555-") as tmp_name:
        tmp = Path(tmp_name)
        stub = tmp / "gh"
        stub.write_text('#!/bin/sh\ncat "$GH_META_FILE"\n', encoding="utf-8")
        stub.chmod(0o700)
        normal = {
            "origin": "artifact-run", "operation": "triage",
            "run_id": "37696663078",
            "artifact_name": "rafcodephi-dualarch-signed-apt-abc",
            "expected_sha": GOOD_SHA,
            "event": "pull_request",
            "path": ".github/workflows/rafcodephi-publish-dev-apt.yml",
            "head_repo": "rafaelmeloreisnovo/termux-packages",
            "head_branch": "hotfix/rafcodephi-apt-blocker-false-positive-20261007",
            "expect_pass": True,
        }
        verify_case(script, tmp, "historical-pr-exact-head", **normal)
        verify_case(script, tmp, "pr-wrong-sha",
                    **{**normal, "expected_sha": "8" * 40, "expect_pass": False})
        verify_case(script, tmp, "pr-foreign-head",
                    **{**normal, "head_repo": "untrusted/fork", "expect_pass": False})
        verify_case(script, tmp, "unrecognized-workflow",
                    **{**normal, "path": ".github/workflows/arbitrary.yml", "expect_pass": False})
        verify_case(script, tmp, "unsupported-event",
                    **{**normal, "event": "schedule", "expect_pass": False})
        verify_case(script, tmp, "dispatch-main",
                    **{**normal, "event": "workflow_dispatch", "head_branch": "main",
                       "expected_sha": "", "expect_pass": True})
        verify_case(script, tmp, "dispatch-nonmain",
                    **{**normal, "event": "workflow_dispatch", "head_branch": "feature",
                       "expected_sha": "", "expect_pass": False})
        verify_case(script, tmp, "source-only",
                    **{**normal, "origin": "source-contract", "operation": "inventory",
                       "run_id": "0", "artifact_name": "", "expected_sha": "",
                       "expect_pass": True})
        verify_case(script, tmp, "source-invalid-run",
                    **{**normal, "origin": "source-contract", "operation": "inventory",
                       "run_id": "37696663078", "artifact_name": "", "expected_sha": "",
                       "expect_pass": False})
    print("RAFCODEPHI_555_WORKFLOW_CONTRACT=PASS cases=9 no_build=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
