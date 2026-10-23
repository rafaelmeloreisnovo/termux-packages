#!/usr/bin/env python3
"""Fail-closed, bounded read-only scan of an exact source-built bootstrap ZIP.

This host-side orchestrator complements the C freestanding byte sentinel.
It does not rewrite ELF files, modify ZIPs, publish packages, or prove runtime.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys
import zipfile

SCHEMA = "rafcodephi.bootstrap-prefix-gate/v1"
PACKAGE = "com.termux.rafacodephi"
PREFIX = "/data/data/com.termux.rafacodephi/files/usr"
LEGACY = b"/data/data/com.termux/"
CHUNK = 64 * 1024
MAX_ENTRY_BYTES = 384 * 1024 * 1024
MAX_TOTAL_BYTES = 1024 * 1024 * 1024
MANDATORY = ("BOOTSTRAP_INFO", "BOOTSTRAP_PROFILE.json", "SYMLINKS.txt",
             "bin/apt", "bin/apt-get", "bin/dpkg", "bin/pkg", "bin/bash")


def file_sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as inp:
        while block := inp.read(CHUNK):
            hasher.update(block)
    return hasher.hexdigest()


def clean_name(name: str) -> str:
    normalized = name.removeprefix("./")
    parts = PurePosixPath(normalized).parts
    if (not normalized or normalized.startswith("/") or "\\" in normalized
            or normalized.rstrip("/") != PurePosixPath(normalized).as_posix()
            or any(p in ("", ".", "..") for p in parts)
            or "/../" in normalized or "//" in normalized):
        raise ValueError("unsafe ZIP member name")
    return normalized


def scan_entry(inp, size: int) -> tuple[bool, bool]:
    """Return (binary, forbidden_legacy_path); bounded streaming with overlap."""
    if size > MAX_ENTRY_BYTES:
        raise ValueError("member size exceeds bounded per-entry budget")
    previous = b""
    binary = False
    seen = False
    while block := inp.read(CHUNK):
        binary |= b"\0" in block or block.startswith(b"\x7fELF")
        seen |= LEGACY in previous + block
        previous = block[-(len(LEGACY) - 1):]
    return binary, seen


def gate(zip_path: Path, arch: str) -> dict:
    result = {
        "schema": SCHEMA, "state": "BLOCKED", "reason": "NOT_EVALUATED",
        "package_name": PACKAGE, "prefix": PREFIX, "arch": arch,
        "archive_sha256": file_sha256(zip_path),
        "entries_checked": 0, "binary_entries_checked": 0,
        "legacy_prefix_entries": [],
        "claim_allowed": False, "release_allowed": False,
        "device_runtime": "TOKEN_VAZIO",
        "package_repo_runtime_state": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
    }
    with zipfile.ZipFile(zip_path, "r") as archive:
        names = set()
        total = 0
        for member in archive.infolist():
            name = clean_name(member.filename)
            if member.is_dir():
                continue
            if name in names:
                raise ValueError("duplicate normalized ZIP member")
            names.add(name)
            total += member.file_size
            if total > MAX_TOTAL_BYTES:
                raise ValueError("uncompressed ZIP size exceeds bounded budget")
        missing = sorted(set(MANDATORY) - names)
        if missing:
            raise ValueError("required entries missing: " + ", ".join(missing))
        for marker_name in ("BOOTSTRAP_INFO", "BOOTSTRAP_PROFILE.json",
                            "SYMLINKS.txt"):
            if archive.getinfo(marker_name).file_size > 64 * 1024:
                raise ValueError("unbounded bootstrap metadata")
        info = archive.read("BOOTSTRAP_INFO").decode("utf-8")
        profile = json.loads(archive.read("BOOTSTRAP_PROFILE.json"))
        for marker in ("RAFCODEPHI_BOOTSTRAP_PROFILE=real-pkg",
                       "RAFCODEPHI_PACKAGE_LAYER=real-pkg",
                       "RAFCODEPHI_CLAIM_ALLOWED=0"):
            if marker not in info:
                raise ValueError("real source-built marker missing: " + marker)
        expected = {"profile": "real-pkg", "package_layer": "real-pkg",
                    "package_name": PACKAGE, "prefix": PREFIX, "arch": arch}
        for key, value in expected.items():
            if profile.get(key) != value:
                raise ValueError("bootstrap profile mismatch: " + key)
        if profile.get("legacy_prefix_forbidden") is not True:
            raise ValueError("legacy-prefix-deny policy missing")
        if profile.get("device_validation") != "TOKEN_VAZIO":
            raise ValueError("runtime proof invented or unrecognized")
        if profile.get("claim_allowed") is not False:
            raise ValueError("unexpected claim promotion")
        for member in archive.infolist():
            if member.is_dir():
                continue
            name = clean_name(member.filename)
            with archive.open(member, "r") as inp:
                binary, legacy_found = scan_entry(inp, member.file_size)
            result["entries_checked"] += 1
            if binary:
                result["binary_entries_checked"] += 1
            # Historical documents may quote the old path, but runtime data
            # must never carry it into the installed prefix.
            documentary = name.startswith(
                ("share/doc/", "share/man/", "share/licenses/")
            )
            if legacy_found and (binary or not documentary):
                result["legacy_prefix_entries"].append(name)
        if result["legacy_prefix_entries"]:
            result["reason"] = "SOURCE_REBUILD_REQUIRED_LEGACY_PREFIX"
        else:
            result["reason"] = "SOURCE_PAYLOAD_PREFIX_AUDITED_RUNTIME_UNVERIFIED"
            result["state"] = "PASS_SCOPED"
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--zip", type=Path, required=True)
    parser.add_argument("--arch", choices=("arm", "aarch64"), required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = gate(args.zip, args.arch)
    except (OSError, ValueError, KeyError, RuntimeError, zipfile.BadZipFile,
            UnicodeDecodeError, json.JSONDecodeError) as err:
        report = {
            "schema": SCHEMA, "state": "BLOCKED",
            "reason": type(err).__name__ + ": " + str(err),
            "package_name": PACKAGE, "arch": args.arch,
            "device_runtime": "TOKEN_VAZIO",
            "claim_allowed": False, "release_allowed": False,
        }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n",
                            encoding="utf-8")
    print("RAFCODEPHI_PREFIX_GATE=" + report["state"]
          + " reason=" + report["reason"]
          + " receipt=" + str(args.receipt))
    if report["state"] != "PASS_SCOPED":
        print(json.dumps(report, sort_keys=True)[:3000], file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
