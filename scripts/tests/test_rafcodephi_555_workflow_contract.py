#!/usr/bin/env python3
"""Exact Bash dispatch contract for RAFCODEPHI 555; no network/build."""
from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/555-artifact-manifold.yml"
REPO = "rafaelmeloreisnovo/termux-packages"

def require(cond: bool, reason: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_555_WORKFLOW_CONTRACT=BLOCKED:{reason}")

def mock_record(event="workflow_dispatch", branch="main", repo=REPO,
                head_repo=REPO, conclusion="failure", path=".github/workflows/rafcodephi-publish-dev-apt.yml"):
    return {
        "repository": {"full_name": repo},
        "head_repository": {"full_name": head_repo},
        "event": event,
        "head_branch": branch,
        "conclusion": conclusion,
        "path": path,
        "head_sha": "a" * 40,
    }

def validate(shell_script: str, event: dict, permit_preview: bool, expected: bool) -> None:
    with tempfile.TemporaryDirectory(prefix="rafcodephi-555-gate-") as base:
        root = Path(base)
        metadata = root / "run.json"
        out = root / "github-output"
        metadata.write_text(json.dumps(event), encoding="utf-8")
        env = dict(os.environ)
        env.update({
            "GITHUB_REPOSITORY": REPO,
            "GITHUB_OUTPUT": str(out),
            "FAKE_METADATA": str(metadata),
            "ORIGIN": "artifact-run",
            "OPERATION": "triage",
            "RUN_ID": "37696663078",
            "ARTIFACT_NAME": "rafcodephi-dualarch-source-build-b1d029649351232649f4b1a03eab2084d0fbc4b6",
            "PERMIT_PREVIEW": "true" if permit_preview else "false",
        })
        stub = 'gh() { cat "$FAKE_METADATA"; }\n'
        run = subprocess.run(
            ["bash", "-c", stub + shell_script],
            env=env, capture_output=True, text=True, check=False,
        )
        require((run.returncode == 0) == expected,
                f"unexpected_gate_result:exit={run.returncode}:stderr={run.stderr[-350:]}")
        if expected:
            require(out.is_file() and "producer_sha=" + "a" * 40 in out.read_text(),
                    "producer_sha_output_missing")

def main() -> int:
    model = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    jobs = model["jobs"]
    workflow = model["on"] if "on" in model else model.get(True)
    opts = workflow["workflow_dispatch"]["inputs"]
    require(opts["permit_preview"]["default"] is False, "preview_must_opt_in")
    compose_steps = jobs["compose"]["steps"]
    script = next(s["run"] for s in compose_steps if s.get("id") == "resolve")
    all_run_scripts = "\n".join(s.get("run", "") for s in compose_steps)
    require('if [[ "$ORIGIN" == "source-contract" ]]; then' in script, "source_branch_bash_guard")
    require('[[ "$RUN_ID" =~ ^[1-9][0-9]*$ ]]' in script, "run_id_bash_guard")
    require('[[ "$ARTIFACT_NAME" =~ ^[A-Za-z0-9._,-]{1,160}$ ]]' in script, "artifact_name_bash_guard")
    require('[[ "$producer_sha" =~ ^[a-f0-9]{40}$ ]]' in script, "sha_bash_guard")
    require('if [[ "$ORIGIN" == artifact-run ]]; then root=inbound; fi' in all_run_scripts, "download_root_bash_guard")
    require('if ${{' not in all_run_scripts and '${{  "' not in all_run_scripts, "malformed_expression_in_shell")
    require("rafcodephi-publish-dev-apt.yml" in script, "signed_apt_workflow_not_allowlisted")
    require('.head_repository.full_name == $repo' in script, "untrusted_repo_guard_missing")
    require("PERMIT_PREVIEW" in script and "pull_request" in script, "preview_scope_missing")
    require(not any(x in all_run_scripts for x in (
        "./scripts/build-rafcodephi-real-bootstrap.sh",
        "bash ./scripts/build-rafcodephi-live-bootstrap.sh",
        "run-rafcodephi-bootstrap-docker.sh",
        "docker build",
    )), "implicit_rebuild_in_555")
    for s in compose_steps:
        if s.get("run") and s.get("shell") == "bash":
            syntax = subprocess.run(["bash", "-n", "-c", s["run"]],
                                    capture_output=True, text=True, check=False)
            require(syntax.returncode == 0, "bash_syntax:" + s.get("name", "unknown") + ":" + syntax.stderr[-150:])
    validate(script, mock_record(), False, True)
    validate(script, mock_record(event="pull_request", branch="hotfix/apt"), False, False)
    validate(script, mock_record(event="pull_request", branch="hotfix/apt"), True, True)
    validate(script, mock_record(event="pull_request", branch="hotfix/apt", head_repo="other/fork"), True, False)
    validate(script, mock_record(event="pull_request", branch="hotfix/apt", repo="other/repo"), True, False)
    validate(script, mock_record(event="pull_request", branch="hotfix/apt", conclusion=None), True, False)
    validate(script, mock_record(path=".github/workflows/unknown.yml"), True, False)
    print("RAFCODEPHI_555_WORKFLOW_CONTRACT=PASS cases=7 build=NOT_RUN")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
