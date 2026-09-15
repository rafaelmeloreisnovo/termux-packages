#!/usr/bin/env python3
"""Trusted control-tree runner: approved candidate -> checks -> build -> receipt.

Runs from the control checkout, outside the candidate tree. Has no publish path.
The receipt qualifies a package for manual review only; Android remains unknown.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import time

from validate_rafcodephi_webhook_event import (
    InvalidEvent, ROOT, bind_candidate, canonical, git, load_policy, require,
    strict_json, validate_event, write_json,
)


def file_sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def deb_identity(path):
    result = subprocess.run(["dpkg-deb", "--field", str(path), "Package", "Architecture"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=15, text=True)
    require(result.returncode == 0, "deb_invalid")
    fields = dict(line.split(": ", 1) for line in result.stdout.splitlines() if ": " in line)
    require(set(fields) == {"Package", "Architecture"}, "deb_metadata")
    return fields


def artifacts_for(root, event):
    root = Path(root).resolve()
    output = root / "output"
    require(output.is_dir() and not output.is_symlink(), "deb_output_missing")
    artifacts = []
    for path in sorted(output.glob("*.deb")):
        require(path.is_file() and not path.is_symlink() and path.resolve().parent == output, "deb_file_type")
        identity = deb_identity(path)
        require(identity["Architecture"] in (event["architecture"], "all"), "deb_wrong_architecture")
        artifacts.append({"path": str(path.relative_to(root)), "sha256": file_sha256(path),
                          "bytes": path.stat().st_size, "package": identity["Package"],
                          "architecture": identity["Architecture"]})
    require(any(a["package"] == event["package"] for a in artifacts), "requested_package_missing")
    return artifacts


def verify_artifacts(root, event, expected):
    require(isinstance(expected, list) and bool(expected), "artifact_manifest_empty")
    require(artifacts_for(root, event) == expected, "artifact_digest_or_identity_changed")


def run_stage(name, argv, root, reports, env, timeout):
    path = reports / f"{name}.log"
    with path.open("wb") as stream:
        try:
            result = subprocess.run(argv, cwd=root, env=env, stdout=stream, stderr=subprocess.STDOUT,
                                    check=False, timeout=timeout)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
        except OSError:
            code = 126
    return {"name": name, "argv": argv, "exit_code": code,
            "status": "PASS" if code == 0 else "FAIL", "log": path.name, "log_sha256": file_sha256(path)}


def build_candidate(intake, candidate_root, reports):
    policy = load_policy()
    require(intake.get("schema") == "rafcodephi.webhook-intake-receipt/v1"
            and intake.get("status") == "INTAKE_ACCEPTED" and intake.get("claim_allowed") is False
            and intake.get("publish_allowed") is False, "intake_receipt")
    require(intake.get("policy_sha256") == hashlib.sha256(canonical(policy)).hexdigest(), "policy_changed")
    event = validate_event(intake["candidate"], policy, max_age=policy["max_queue_age_seconds"])
    identity = bind_candidate(event, ROOT, intake["trusted_commit"])
    require(identity["recipe_sha256"] == intake["recipe_sha256"], "intake_recipe_changed")
    root = Path(candidate_root).resolve()
    require(git(root, "rev-parse", "HEAD").decode().strip() == event["commit_sha"], "candidate_checkout_mismatch")
    recipe = root / identity["recipe_path"]
    require(not recipe.is_symlink() and file_sha256(recipe) == identity["recipe_sha256"], "candidate_recipe_changed")
    require(git(root, "status", "--porcelain", "--untracked-files=all").strip() == b"", "candidate_checkout_dirty")
    require(not (root / "output").is_symlink() and not list((root / "output").glob("*.deb")), "stale_deb_output")
    reports = Path(reports).resolve()
    require(reports != root and root not in reports.parents, "reports_must_be_outside_candidate")
    reports.mkdir(parents=True, exist_ok=True)
    # No PAT, GitHub token, Actions credentials, or arbitrary caller build variables.
    env = {key: os.environ[key] for key in ("PATH", "HOME", "LANG", "TMPDIR") if key in os.environ}
    env.update({"CI": "true", "TERMUX_DOCKER_USE_SUDO": "1", "PYTHONDONTWRITEBYTECODE": "1"})
    commands = [
        ("package-provenance", ["python3", "scripts/validate_package_provenance_handoff.py"], 60),
        ("federated-provenance", ["python3", "scripts/validate_rafcodephi_federated_provenance.py"], 60),
        ("recipe-syntax", ["bash", "-n", identity["recipe_path"]], 30),
        ("manifest-compile", ["make", "-C", "core", "termux-build-core"], 120),
        ("manifest-fail-closed", ["python3", "core/tests/test_manifest_fail_closed.py"], 120),
        ("package-build", ["./scripts/run-docker.sh", "./build-package-rafcodephi.sh", "-C", "-a",
                           event["architecture"], event["package"]], 7200),
    ]
    receipt = {"schema": "rafcodephi.webhook-build-receipt/v1", "candidate": event, **identity,
               "intake_sha256": hashlib.sha256(canonical(intake)).hexdigest(),
               "observed_at": int(time.time()), "stages": [], "artifacts": [], "status": "BLOCKED",
               "device_runtime": "TOKEN_VAZIO", "production_signing_root": "TOKEN_VAZIO",
               "promotion": "BLOCKED", "publish_allowed": False, "claim_allowed": False}
    for name, argv, timeout in commands:
        stage = run_stage(name, argv, root, reports, env, timeout)
        receipt["stages"].append(stage)
        if stage["status"] != "PASS":
            break
    if len(receipt["stages"]) == len(commands) and all(s["status"] == "PASS" for s in receipt["stages"]):
        try:
            receipt["artifacts"] = artifacts_for(root, event)
            verify_artifacts(root, event, receipt["artifacts"])
            receipt.update(status="BUILD_TEST_PASS", promotion="MANUAL_REVIEW_REQUIRED")
        except (InvalidEvent, OSError, subprocess.SubprocessError):
            receipt["artifact_gate"] = "FAIL"
    write_json(reports / "receipt.json", receipt)
    (reports / "receipt.json.sha256").write_text(file_sha256(reports / "receipt.json") + "  receipt.json\n", encoding="ascii")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-root", type=Path, required=True)
    parser.add_argument("--reports", type=Path, required=True)
    args = parser.parse_args()
    try:
        intake = strict_json(os.environ.get("INTAKE_RECEIPT_JSON", "").encode("utf-8"))
        receipt = build_candidate(intake, args.candidate_root, args.reports)
        passed = receipt["status"] == "BUILD_TEST_PASS"
        print(f"WEBHOOK_BUILD={receipt['status']} promotion={receipt['promotion']} claim_allowed=false")
        return 0 if passed else 1
    except (InvalidEvent, OSError, subprocess.SubprocessError, KeyError, TypeError):
        write_json(args.reports / "receipt.json", {"schema": "rafcodephi.webhook-build-receipt/v1",
                   "status": "BLOCKED", "reason": "candidate_binding_or_io_failure", "promotion": "BLOCKED",
                   "publish_allowed": False, "claim_allowed": False})
        print("WEBHOOK_BUILD=BLOCKED candidate_binding_or_io_failure claim_allowed=false")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
