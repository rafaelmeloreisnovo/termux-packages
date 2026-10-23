#!/usr/bin/env python3
"""Source-built .deb custody and legacy-prefix audit; no mutation of inputs.

This host-side validator requires a complete SHA256SUMS file, audits tar members
and bounded data streams, then emits a scoped receipt. It does NOT publish APT.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile

from rafcodephi_bootstrap_prefix_gate import (
    LEGACY, PACKAGE, PREFIX, scan_entry, file_sha256
)

SCHEMA = "rafcodephi.deb-set-prefix-gate/v1"
MAX_PACKAGES = 4096


def indexed_files(directory: Path) -> list[tuple[Path, str]]:
    manifest = directory / "SHA256SUMS"
    if not manifest.is_file():
        raise ValueError("SHA256SUMS missing")
    expected = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (\S+\.deb)", line)
        if not match:
            raise ValueError("malformed SHA256SUMS")
        sha, name = match.groups()
        if Path(name).name != name or name in expected:
            raise ValueError("unsafe or duplicate SHA256SUMS member")
        expected[name] = sha
    actual = {file.name for file in directory.glob("*.deb") if file.is_file()}
    if not actual or actual != set(expected) or len(actual) > MAX_PACKAGES:
        raise ValueError("package inventory mismatch or empty set")
    for name, sha in expected.items():
        if file_sha256(directory / name) != sha:
            raise ValueError("deb SHA256 mismatch: " + name)
    return [(directory / name, expected[name]) for name in sorted(expected)]


def inspect_deb(path: Path) -> list[str]:
    risks = []
    proc = subprocess.Popen(
        ["dpkg-deb", "--fsys-tarfile", str(path)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    try:
        assert proc.stdout is not None
        with tarfile.open(fileobj=proc.stdout, mode="r|*") as tar:
            for item in tar:
                name = item.name.removeprefix("./")
                if (name.startswith("/") or name.startswith("../") or
                        "/../" in name or "//" in name):
                    raise ValueError("unsafe deb tar member name")
                if name.startswith("data/data/com.termux/"):
                    risks.append(name)
                if item.issym() or item.islnk():
                    if LEGACY in item.linkname.encode():
                        risks.append(name + " -> " + item.linkname)
                if not item.isfile():
                    continue
                source = tar.extractfile(item)
                if source is None:
                    raise ValueError("unreadable deb tar member")
                with source:
                    binary, legacy = scan_entry(source, item.size)
                runtime_path = ("/bin/" in name or "/lib/" in name or
                                "/libexec/" in name or "/etc/apt/" in name)
                if legacy and (binary or runtime_path):
                    risks.append(name)
        code = proc.wait()
        stderr = proc.stderr.read().decode("utf-8", "replace") if proc.stderr else ""
        if code != 0:
            raise ValueError("dpkg-deb extraction blocked: " + stderr[:160])
    except Exception:
        proc.kill() if proc.poll() is None else None
        proc.wait()
        raise
    finally:
        if proc.stdout:
            proc.stdout.close()
        if proc.stderr:
            proc.stderr.close()
    return risks


def gate(directory: Path, arch: str) -> dict:
    packages = indexed_files(directory)
    report = {
        "schema": SCHEMA, "arch": arch, "package_name": PACKAGE,
        "prefix": PREFIX, "deb_count": len(packages),
        "sha256sums_sha256": file_sha256(directory / "SHA256SUMS"),
        "legacy_prefix_findings": [],
        "state": "BLOCKED", "claim_allowed": False, "release_allowed": False,
        "device_runtime": "TOKEN_VAZIO",
        "package_repo_runtime_state": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
    }
    for path, digest in packages:
        risk = inspect_deb(path)
        if risk:
            report["legacy_prefix_findings"].append({
                "file": path.name, "sha256": digest, "entries": sorted(set(risk))
            })
    if not report["legacy_prefix_findings"]:
        report["state"] = "PASS_SCOPED"
    return report


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--deb-dir", type=Path, required=True)
    p.add_argument("--arch", choices=("arm", "aarch64"), required=True)
    p.add_argument("--receipt", type=Path, required=True)
    args = p.parse_args()
    try:
        report = gate(args.deb_dir, args.arch)
    except (ValueError, OSError, subprocess.SubprocessError, tarfile.TarError) as e:
        report = {
            "schema": SCHEMA, "state": "BLOCKED",
            "reason": type(e).__name__ + ": " + str(e),
            "device_runtime": "TOKEN_VAZIO",
            "claim_allowed": False, "release_allowed": False,
        }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n")
    print("RAFCODEPHI_DEB_PREFIX_GATE=" + report["state"])
    if report["state"] != "PASS_SCOPED":
        print(json.dumps(report, sort_keys=True)[:3000], file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
