#!/usr/bin/env python3
"""Fail-closed, source-only per-lot custody for RAFCODEPHI ARM/aarch64 builds.

SOURCE_ONLY receipts: they never prove a signed APT repo, install, or device.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

from rafcodephi_deb_set_gate import indexed_files

ARCHES = ("arm", "aarch64")
SCHEMA = "rafcodephi.source-lot-custody/v1"


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def rows(path: Path) -> dict[str, str]:
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        key, sep, value = line.partition("=")
        if not sep or not key or key in result:
            raise ValueError("invalid or duplicate manifest key")
        result[key] = value
    return result


def audit(lot: Path, arch: str, commit: str) -> dict:
    if arch not in ARCHES or len(commit) != 40 or any(c not in "0123456789abcdef" for c in commit):
        raise ValueError("invalid architecture or source SHA")
    needed = {
        "manifest": lot / "manifest.txt",
        "zip": lot / "bootstrap.zip",
        "prefix": lot / "prefix-gate.json",
        "deb": lot / "deb-gate.json",
    }
    for label, file in needed.items():
        if not file.is_file() or file.is_symlink() or file.stat().st_size == 0:
            raise ValueError(f"missing/unsafe lot file: {label}")
    manifest = rows(needed["manifest"])
    if not (manifest.get("schema") == "rafcodephi.real-bootstrap-sourcebuild/v1"
            and manifest.get("package_name") == "com.termux.rafacodephi"
            and manifest.get("prefix") == "/data/data/com.termux.rafacodephi/files/usr"
            and manifest.get("bridge_allowed") == "false"
            and manifest.get("legacy_prefix_allowed") == "false"
            and manifest.get("device_runtime_proof") == "TOKEN_VAZIO"):
        raise ValueError("source manifest contract mismatch")
    for gate_name, fkey in (("prefix", "prefix_gate"), ("deb", "deb_gate")):
        report = json.loads(needed[gate_name].read_text(encoding="utf-8"))
        if not (report.get("state") == "PASS_SCOPED"
                and report.get("arch") == arch
                and report.get("claim_allowed") is False
                and report.get("device_runtime") == "TOKEN_VAZIO"
                and manifest.get(f"{fkey}_state_{arch}") == "PASS_SCOPED"
                and manifest.get(f"{fkey}_receipt_sha256_{arch}") == sha256(needed[gate_name])):
            raise ValueError(f"{gate_name} integrity gate mismatch")
    debdir = lot / "debs"
    if not debdir.is_dir() or debdir.is_symlink():
        raise ValueError("unsafe/missing DEB directory")
    records = indexed_files(debdir)  # canonical paths, all SHA256, no aliases
    if (manifest.get(f"deb_count_{arch}") != str(len(records))
            or manifest.get(f"deb_set_sha256_{arch}") != sha256(debdir / "SHA256SUMS")):
        raise ValueError("DEB inventory receipt mismatch")
    with zipfile.ZipFile(needed["zip"]) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)) or archive.testzip() is not None:
            raise ValueError("duplicate/corrupt ZIP member")
        if ("BOOTSTRAP_PROFILE.json" not in names or "BOOTSTRAP_INFO" not in names):
            raise ValueError("ZIP source contract missing")
    if manifest.get(f"bytes_{arch}") != str(needed["zip"].stat().st_size):
        raise ValueError("ZIP size mismatch")
    leaves = {p.name: digest for p, digest in records}
    root = hashlib.sha256(json.dumps(leaves, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return {
        "schema": SCHEMA, "architecture": arch, "source_commit": commit,
        "state": "PASS_SCOPED", "deb_count": len(records),
        "deb_records_root_sha256": root,
        "sha256sums_sha256": sha256(debdir / "SHA256SUMS"),
        "bootstrap_sha256": sha256(needed["zip"]),
        "manifest_sha256": sha256(needed["manifest"]),
        "prefix_gate_sha256": sha256(needed["prefix"]),
        "deb_gate_sha256": sha256(needed["deb"]),
        "signed_apt_repository": "NOT_RUN",
        "device_runtime": "TOKEN_VAZIO", "claim_allowed": False,
    }


def aggregate(arm: Path, aarch64: Path, commit: str) -> dict:
    records = {arch: audit(root, arch, commit)
               for arch, root in (("arm", arm), ("aarch64", aarch64))}
    canonical = json.dumps(records, sort_keys=True, separators=(",", ":")).encode()
    return {
        "schema": "rafcodephi.source-batches-custody/v1",
        "source_commit": commit, "state": "PASS_SCOPED",
        "architectures": records,
        "merkle_candidate_sha256": hashlib.sha256(canonical).hexdigest(),
        "signed_apt_repository": "NOT_RUN",
        "publish": "BLOCKED", "device_runtime": "TOKEN_VAZIO",
        "claim_allowed": False,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-commit", required=True)
    ap.add_argument("--arch", choices=ARCHES)
    ap.add_argument("--lot", type=Path)
    ap.add_argument("--arm-lot", type=Path)
    ap.add_argument("--aarch64-lot", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    try:
        if args.arch and args.lot and not args.arm_lot and not args.aarch64_lot:
            receipt = audit(args.lot, args.arch, args.source_commit)
        elif args.arm_lot and args.aarch64_lot and not args.arch and not args.lot:
            receipt = aggregate(args.arm_lot, args.aarch64_lot, args.source_commit)
        else:
            ap.error("choose either --arch + --lot, or --arm-lot + --aarch64-lot")
        rc = 0
    except (OSError, ValueError, KeyError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        receipt = {
            "schema": SCHEMA, "state": "BLOCKED",
            "reason": f"{type(exc).__name__}: {exc}",
            "source_commit": args.source_commit,
            "device_runtime": "TOKEN_VAZIO", "claim_allowed": False,
        }
        rc = 1
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(f"RAFCODEPHI_BATCH_CUSTODY={receipt['state']}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
