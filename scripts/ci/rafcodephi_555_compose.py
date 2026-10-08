#!/usr/bin/env python3
"""555: bounded, observation-only artifact re-composition; never a build/release gate."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import tempfile
import zipfile
from pathlib import Path

ROOT_FILES = (
    "scripts/build-rafcodephi-real-bootstrap.sh",
    "scripts/run-rafcodephi-bootstrap-docker.sh",
    "scripts/ci/rafcodephi_failure_triage.py",
)
HEX40 = re.compile(r"^[a-f0-9]{40}$")
MAX_FILES = 10000
MAX_TOTAL_BYTES = 10 * (1 << 30)


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def inventory(root: Path) -> dict[str, dict]:
    if not root.is_dir():
        raise ValueError("input directory missing")
    files: dict[str, dict] = {}
    total = 0
    for p in sorted(root.rglob("*")):
        if p.is_symlink():
            raise ValueError("symlink input forbidden: " + str(p))
        if not p.is_file():
            continue
        relative = p.relative_to(root).as_posix()
        total += p.stat().st_size
        if len(files) >= MAX_FILES or total > MAX_TOTAL_BYTES:
            raise ValueError("input inventory limit exceeded")
        files[relative] = {"sha256": digest(p), "bytes": p.stat().st_size}
    if not files:
        raise ValueError("empty source inventory")
    return files


def verify_handoff(root: Path, files: dict, producer_sha: str, run_id: int) -> str:
    matching = [p for p in root.rglob("rafcodephi-auto-handoff.json") if p.is_file()]
    if len(matching) > 1:
        raise ValueError("ambiguous producer handoff receipts")
    if not matching:
        return "RUN_BOUND_HASHED_UNATTESTED"
    receipt = json.loads(matching[0].read_text(encoding="utf-8"))
    if receipt.get("producer_commit") != producer_sha or receipt.get("producer_run_id") != run_id:
        raise ValueError("producer run/SHA receipt mismatch")
    hashes = receipt.get("sha256")
    if not isinstance(hashes, dict) or not hashes:
        raise ValueError("producer SHA-256 map missing")
    for relative, expected in hashes.items():
        if relative not in files or files[relative]["sha256"] != expected:
            raise ValueError("producer file digest mismatch: " + relative)
    return "HANDOFF_SHA256_VERIFIED"


def compose(origin: str, operation: str, output_mode: str, root: Path, out: Path,
            run_id: int, producer_sha: str, current_sha: str, artifact_name: str) -> dict:
    if origin not in ("source-contract", "artifact-run"):
        raise ValueError("invalid origin")
    if operation not in ("inventory", "triage", "compose"):
        raise ValueError("invalid operation")
    if output_mode not in ("receipt", "zipraf"):
        raise ValueError("invalid output mode")
    if not HEX40.fullmatch(current_sha):
        raise ValueError("current commit SHA missing/invalid")
    if origin == "source-contract":
        if operation != "inventory" or run_id or producer_sha or artifact_name:
            raise ValueError("source-contract permits inventory only")
        files = {name: {"sha256": digest(root / name), "bytes": (root / name).stat().st_size}
                 for name in ROOT_FILES}
        custody = "CHECKOUT_EXACT_SHA"
        observed_sha = current_sha
    else:
        if run_id <= 0 or not HEX40.fullmatch(producer_sha) or not artifact_name:
            raise ValueError("artifact requires exact run ID, producer SHA and name")
        files = inventory(root)
        custody = verify_handoff(root, files, producer_sha, run_id)
        observed_sha = producer_sha
        manifests = [p for p in files if p.endswith("RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt")]
        for name in manifests:
            payload = (root / name).read_text(encoding="utf-8")
            if ("claim_allowed_device_runtime=false" not in payload or
                    "device_runtime_proof=TOKEN_VAZIO" not in payload):
                raise ValueError("bootstrap manifest missing fail-closed device fields")
    triage = None
    if operation == "triage":
        logs = sorted((p for p in files if p.endswith(".log")),
                      key=lambda p: ("bootstrap" not in p, p))
        if not logs:
            raise ValueError("triage requested but no log in source artifact")
        from rafcodephi_failure_triage import scan
        triage = {"log": logs[0], "analysis": scan(root / logs[0])}
    report = {
        "schema": "rafcodephi.555-artifact-manifold/v1",
        "status": "OBSERVED_UNPROMOTED",
        "origin": origin, "operation": operation, "output": output_mode,
        "artifact_name": artifact_name if origin == "artifact-run" else "TOKEN_VAZIO",
        "producer_run_id": run_id if origin == "artifact-run" else "TOKEN_VAZIO",
        "producer_sha": observed_sha,
        "verifier_sha": current_sha,
        "source_commit_differs": observed_sha != current_sha,
        "custody": custody, "files": files,
        "triage": triage if triage is not None else "NOT_APPLICABLE",
        "compiled_in_this_run": False,
        "release_allowed": False, "claim_allowed": False,
        "device_runtime_proof": "TOKEN_VAZIO",
        "rule": "SOURCE!=ARTIFACT!=EXECUTION!=EVIDENCE!=CLAIM",
    }
    out.mkdir(parents=True, exist_ok=True)
    receipt = out / "555-receipt.json"
    receipt.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (out / "555-receipt.json.sha256").write_text(
        digest(receipt) + "  555-receipt.json\n", encoding="utf-8"
    )
    if output_mode == "zipraf":
        # Reference-only packet: no source binary/.deb/bootstrap is copied.
        target = out / "555-references.zip"
        with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
            for src in (receipt, out / "555-receipt.json.sha256"):
                info = zipfile.ZipInfo(src.name, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, src.read_bytes())
        (out / "555-references.zip.sha256").write_text(
            digest(target) + "  555-references.zip\n", encoding="utf-8"
        )
    return report


def workflow_contract_self_test() -> None:
    """Apt re-use must parse Bash variables in shell, not GitHub expressions."""
    workflow = Path(__file__).resolve().parents[2] / ".github/workflows/555-artifact-manifold.yml"
    source = workflow.read_text(encoding="utf-8")
    for required in (
        'if [[ "$ORIGIN" == source-contract ]]; then',
        '[[ "$RUN_ID" =~ ^[1-9][0-9]*$ ]]',
        '[[ "$ARTIFACT_NAME" =~ ^[A-Za-z0-9._,-]{1,160}$ ]]',
        '[[ "$producer_sha" =~ ^[a-f0-9]{40}$ ]]',
        'if [[ "$ORIGIN" == artifact-run ]]; then root=inbound; fi',
        "'.github/workflows/rafcodephi-publish-dev-apt.yml'",
    ):
        if required not in source:
            raise AssertionError(f"555_WORKFLOW_CONTRACT_MISSING:{required}")
    expression_prefix = "$" + "{{"
    if ("if " + expression_prefix) in source or (expression_prefix + '  "') in source:
        raise AssertionError("555_BASH_VARIABLES_IN_GITHUB_EXPRESSION")
    print("RAFCODEPHI_555_WORKFLOW_CONTRACT=PASS")


def self_test() -> None:

    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        checkout = base / "repo"
        for name in ROOT_FILES:
            p = checkout / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("source\n", encoding="utf-8")
        out = base / "out"
        sha = "a" * 40
        one = compose("source-contract", "inventory", "receipt", checkout, out,
                      0, "", sha, "")
        assert one["custody"] == "CHECKOUT_EXACT_SHA" and not one["compiled_in_this_run"]
        inbound = base / "inbound"
        payload = inbound / "artifacts" / "example.bin"
        payload.parent.mkdir(parents=True)
        payload.write_bytes(b"immutable")
        log = inbound / "build" / "reports" / "sample-bootstrap.log"
        log.parent.mkdir(parents=True)
        log.write_text("old apt blocker still active\n", encoding="utf-8")
        handoff = log.parent / "rafcodephi-auto-handoff.json"
        handoff.write_text(json.dumps({
            "producer_commit": "b" * 40, "producer_run_id": 555,
            "sha256": {"artifacts/example.bin": digest(payload)}
        }), encoding="utf-8")
        two = compose("artifact-run", "triage", "zipraf", inbound, out,
                      555, "b" * 40, sha, "test-artifact")
        assert two["custody"] == "HANDOFF_SHA256_VERIFIED"
        assert two["triage"]["analysis"]["primary_candidate"] == "APT_GUARD"
        assert (out / "555-references.zip").is_file()
        try:
            compose("artifact-run", "inventory", "receipt", inbound, out,
                    556, "b" * 40, sha, "test-artifact")
        except ValueError as exc:
            assert "mismatch" in str(exc)
        else:
            raise AssertionError("mismatched run was accepted")
    workflow_contract_self_test()
    print("RAFCODEPHI_555_SELF_TEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--origin", default="source-contract")
    parser.add_argument("--operation", default="inventory")
    parser.add_argument("--output-mode", default="receipt")
    parser.add_argument("--input-root", type=Path, default=Path("."))
    parser.add_argument("--out", type=Path, default=Path("_555"))
    parser.add_argument("--producer-run-id", type=int, default=0)
    parser.add_argument("--producer-sha", default="")
    parser.add_argument("--current-sha", default="")
    parser.add_argument("--artifact-name", default="")
    a = parser.parse_args()
    if a.self_test:
        self_test()
        return 0
    try:
        r = compose(a.origin, a.operation, a.output_mode, a.input_root, a.out,
                    a.producer_run_id, a.producer_sha, a.current_sha, a.artifact_name)
    except (ValueError, OSError, json.JSONDecodeError) as error:
        a.out.mkdir(parents=True, exist_ok=True)
        (a.out / "555-blocked.json").write_text(json.dumps({
            "schema": "rafcodephi.555-artifact-manifold/v1",
            "state": "BLOCKED", "reason": str(error), "claim_allowed": False
        }, indent=2) + "\n", encoding="utf-8")
        print("RAFCODEPHI_555=BLOCKED " + str(error))
        return 1
    print("RAFCODEPHI_555=" + r["status"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
 in source:
        raise AssertionError("555_BASH_VARIABLES_IN_GITHUB_EXPRESSION")
    print("RAFCODEPHI_555_WORKFLOW_CONTRACT=PASS")


def self_test() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        base = Path(tmp)
        checkout = base / "repo"
        for name in ROOT_FILES:
            p = checkout / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("source\n", encoding="utf-8")
        out = base / "out"
        sha = "a" * 40
        one = compose("source-contract", "inventory", "receipt", checkout, out,
                      0, "", sha, "")
        assert one["custody"] == "CHECKOUT_EXACT_SHA" and not one["compiled_in_this_run"]
        inbound = base / "inbound"
        payload = inbound / "artifacts" / "example.bin"
        payload.parent.mkdir(parents=True)
        payload.write_bytes(b"immutable")
        log = inbound / "build" / "reports" / "sample-bootstrap.log"
        log.parent.mkdir(parents=True)
        log.write_text("old apt blocker still active\n", encoding="utf-8")
        handoff = log.parent / "rafcodephi-auto-handoff.json"
        handoff.write_text(json.dumps({
            "producer_commit": "b" * 40, "producer_run_id": 555,
            "sha256": {"artifacts/example.bin": digest(payload)}
        }), encoding="utf-8")
        two = compose("artifact-run", "triage", "zipraf", inbound, out,
                      555, "b" * 40, sha, "test-artifact")
        assert two["custody"] == "HANDOFF_SHA256_VERIFIED"
        assert two["triage"]["analysis"]["primary_candidate"] == "APT_GUARD"
        assert (out / "555-references.zip").is_file()
        try:
            compose("artifact-run", "inventory", "receipt", inbound, out,
                    556, "b" * 40, sha, "test-artifact")
        except ValueError as exc:
            assert "mismatch" in str(exc)
        else:
            raise AssertionError("mismatched run was accepted")
    print("RAFCODEPHI_555_SELF_TEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--origin", default="source-contract")
    parser.add_argument("--operation", default="inventory")
    parser.add_argument("--output-mode", default="receipt")
    parser.add_argument("--input-root", type=Path, default=Path("."))
    parser.add_argument("--out", type=Path, default=Path("_555"))
    parser.add_argument("--producer-run-id", type=int, default=0)
    parser.add_argument("--producer-sha", default="")
    parser.add_argument("--current-sha", default="")
    parser.add_argument("--artifact-name", default="")
    a = parser.parse_args()
    if a.self_test:
        self_test()
        return 0
    try:
        r = compose(a.origin, a.operation, a.output_mode, a.input_root, a.out,
                    a.producer_run_id, a.producer_sha, a.current_sha, a.artifact_name)
    except (ValueError, OSError, json.JSONDecodeError) as error:
        a.out.mkdir(parents=True, exist_ok=True)
        (a.out / "555-blocked.json").write_text(json.dumps({
            "schema": "rafcodephi.555-artifact-manifold/v1",
            "state": "BLOCKED", "reason": str(error), "claim_allowed": False
        }, indent=2) + "\n", encoding="utf-8")
        print("RAFCODEPHI_555=BLOCKED " + str(error))
        return 1
    print("RAFCODEPHI_555=" + r["status"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
