#!/usr/bin/env python3
"""RAFCODEPHI package-delivery gate.

Two deliberately separate gates:

* source: proves that the existing Termux builder + essential recipes expose the
  required source contract for RAFCODEPHI. It does not claim a package artifact.
* artifact: validates a concrete Debian package with dpkg-deb. Missing artifacts
  are BLOCKED, never promoted from source readiness.

This script does not create a second package builder; it governs build-package.sh.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

PACKAGE = "com.termux.rafacodephi"
PREFIX = f"/data/data/{PACKAGE}/files/usr"
ALLOWED_ARCHES = {"arm", "aarch64"}
SCHEMA = "rafcodephi_delivery_gate/1.0.0"


class GateError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise GateError(f"required source missing/unreadable: {path}: {exc}") from exc


def require(text: str, pattern: str, label: str) -> None:
    if re.search(pattern, text, flags=re.MULTILINE) is None:
        raise GateError(f"source contract missing: {label}")


def source_gate(repo: Path) -> dict[str, Any]:
    builder_p = repo / "build-package.sh"
    bash_p = repo / "packages" / "bash" / "build.sh"
    tools_p = repo / "packages" / "termux-tools" / "build.sh"
    bootstrap_p = repo / "scripts" / "emit_rafcodephi_bootstrap_source_manifest.py"
    repo_json_p = repo / "repo.json"

    builder = read(builder_p)
    bash = read(bash_p)
    tools = read(tools_p)
    bootstrap = read(bootstrap_p)
    repo_json_raw = read(repo_json_p)

    try:
        repo_json = json.loads(repo_json_raw)
    except json.JSONDecodeError as exc:
        raise GateError(f"repo.json invalid JSON: {exc}") from exc

    # Existing builder is authoritative; prove it can produce Debian packages
    # for the two Android architectures relevant to this delivery contract.
    require(builder, r"Build a package by creating a \.deb file in the output/ folder", "builder .deb output contract")
    require(builder, r"aarch64\(default\), arm, i686, x86_64 or all", "builder arm/aarch64 architecture contract")
    require(builder, r"TERMUX_ON_DEVICE_BUILD=true", "on-device build mode")
    require(builder, r"termux_step_create_debian_package\.sh", "Debian package creation stage")
    require(builder, r"--format", "explicit package-format option")

    # Bash is a real essential recipe, not a placeholder.
    require(bash, r"^TERMUX_PKG_VERSION=[^\s]+", "bash version")
    require(bash, r"^TERMUX_PKG_SRCURL=https://", "bash source URL")
    require(bash, r"^TERMUX_PKG_SHA256=[0-9a-f]{64}$", "bash source SHA-256")
    require(bash, r"^TERMUX_PKG_ESSENTIAL=true$", "bash essential flag")
    require(bash, r'TERMUX_PKG_DEPENDS="[^"]*termux-tools[^"]*"', "bash -> termux-tools dependency")

    # termux-tools is the namespace/prefix bridge for the fork.
    require(tools, r"^TERMUX_PKG_ESSENTIAL=true$", "termux-tools essential flag")
    require(tools, r"export TERMUX_PREFIX TERMUX_APP_PACKAGE", "termux-tools exports package/prefix")
    require(tools, r"com\\\.termux", "termux-tools package-id rewrite source")
    require(tools, r"TERMUX_APP_PACKAGE", "termux-tools package-specific rewrite")

    # Bootstrap source manifest pins the same package/prefix contract.
    require(bootstrap, rf'PACKAGE = "{re.escape(PACKAGE)}"', "bootstrap package id")
    require(bootstrap, r'PREFIX = f"/data/data/\{PACKAGE\}/files/usr"', "bootstrap prefix derivation")
    require(bootstrap, r'ARCH_TO_ABI = \{"arm": "armeabi-v7a", "aarch64": "arm64-v8a"\}', "bootstrap ABI mapping")
    require(bootstrap, r'"device_runtime": "TOKEN_VAZIO"', "bootstrap device claim remains empty")

    if not isinstance(repo_json, dict) or "packages" not in repo_json:
        raise GateError("repo.json does not declare packages repository")

    return {
        "schema": SCHEMA,
        "gate": "source",
        "state": "PASS",
        "claim_allowed": False,
        "package": PACKAGE,
        "prefix": PREFIX,
        "architectures": sorted(ALLOWED_ARCHES),
        "package_format": "debian",
        "builder": "build-package.sh",
        "source_contract": {
            "bash_recipe": "PASS",
            "termux_tools_recipe": "PASS",
            "builder_deb_path": "PASS",
            "bootstrap_profile_contract": "PASS",
        },
        "artifact": "NOT_MEASURED",
        "repo_metadata_generated": "NOT_MEASURED",
        "apt_pkg_runtime": "NOT_MEASURED",
        "physical_device_runtime": "NOT_MEASURED",
        "sources": {
            str(p.relative_to(repo)): {"sha256": sha256(p)}
            for p in (builder_p, bash_p, tools_p, bootstrap_p, repo_json_p)
        },
    }


def dpkg_field(path: Path, field: str) -> str:
    try:
        return subprocess.check_output(
            ["dpkg-deb", "-f", str(path), field],
            text=True,
            stderr=subprocess.STDOUT,
        ).strip()
    except (OSError, subprocess.CalledProcessError) as exc:
        raise GateError(f"dpkg-deb field read failed ({field}): {exc}") from exc


def artifact_gate(repo: Path, artifact: Path, expected_package: str, arch: str) -> dict[str, Any]:
    # Source gate must pass first; artifact evidence cannot float without origin.
    source = source_gate(repo)
    if arch not in ALLOWED_ARCHES:
        raise GateError(f"unsupported RAFCODEPHI delivery arch: {arch}")
    if not artifact.is_file():
        raise GateError(f"artifact missing: {artifact}")
    if shutil.which("dpkg-deb") is None:
        raise GateError("dpkg-deb unavailable; artifact validation blocked")

    package = dpkg_field(artifact, "Package")
    deb_arch = dpkg_field(artifact, "Architecture")
    version = dpkg_field(artifact, "Version")
    if package != expected_package:
        raise GateError(f"unexpected package: {package!r} != {expected_package!r}")

    expected_deb_arch = {"arm": "arm", "aarch64": "aarch64"}[arch]
    if deb_arch != expected_deb_arch:
        raise GateError(f"unexpected architecture: {deb_arch!r} != {expected_deb_arch!r}")
    if not version:
        raise GateError("empty Debian Version field")

    try:
        listing = subprocess.check_output(
            ["dpkg-deb", "-c", str(artifact)],
            text=True,
            stderr=subprocess.STDOUT,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise GateError(f"dpkg-deb content listing failed: {exc}") from exc
    if not listing.strip():
        raise GateError("Debian package has empty content listing")

    return {
        "schema": SCHEMA,
        "gate": "artifact",
        "state": "PASS",
        "claim_allowed": False,
        "source_gate": source["state"],
        "artifact": str(artifact),
        "artifact_sha256": sha256(artifact),
        "artifact_size": artifact.stat().st_size,
        "package": package,
        "version": version,
        "arch": arch,
        "debian_architecture": deb_arch,
        "content_listing_nonempty": True,
        "repo_metadata_generated": "NOT_MEASURED",
        "apt_pkg_runtime": "NOT_MEASURED",
        "physical_device_runtime": "NOT_MEASURED",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=("source", "artifact"))
    ap.add_argument("--repo", type=Path, default=Path("."))
    ap.add_argument("--artifact", type=Path)
    ap.add_argument("--expected-package")
    ap.add_argument("--arch", choices=sorted(ALLOWED_ARCHES))
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()

    try:
        if args.mode == "source":
            doc = source_gate(args.repo)
        else:
            if args.artifact is None or args.expected_package is None or args.arch is None:
                ap.error("artifact mode requires --artifact, --expected-package and --arch")
            doc = artifact_gate(args.repo, args.artifact, args.expected_package, args.arch)
    except GateError as exc:
        print(f"DELIVERY_GATE=BLOCKED reason={exc}", file=sys.stderr)
        print("CLAIM_ALLOWED=false", file=sys.stderr)
        return 2

    text = json.dumps(doc, indent=2, sort_keys=True) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    print(f"DELIVERY_{args.mode.upper()}_GATE=PASS", file=sys.stderr)
    print("CLAIM_ALLOWED=false", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
