#!/usr/bin/env python3
"""Lossless ARM32 bootstrap-only export from a sealed RAFCODEPHI GitHub run.

Run this on GitHub Actions after downloading the ARM artifact and custody artifact.
It NEVER rebuilds, relocates, truncates or rewrites the original bootstrap ZIP.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path

PACKAGE = "com.termux.rafacodephi"
PREFIX = f"/data/data/{PACKAGE}/files/usr"
SCHEMA = "rafcodephi.real-bootstrap-sourcebuild/v1"
REQUIRED = {
    "BOOTSTRAP_INFO", "BOOTSTRAP_PROFILE.json", "SYMLINKS.txt",
    "bin/pkg", "bin/apt", "bin/apt-get", "bin/dpkg", "bin/bash",
    "bin/busybox", "bin/proot", "bin/ninja", "bin/sh",
    "var/lib/dpkg/status", "etc/apt/sources.list.d/termux.sources",
    "etc/apt/apt.conf.d/00rafcodephi-repository-block",
}
ELFS = ("bin/apt", "bin/apt-get", "bin/dpkg", "bin/bash", "bin/proot", "bin/ninja", "bin/busybox")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def exactly_one(root: Path, name: str) -> Path:
    matches = [p for p in root.rglob(name) if p.is_file() and not p.is_symlink()]
    if len(matches) != 1:
        raise RuntimeError(f"EXPECTED_ONE_{name}: observed={len(matches)}")
    return matches[0]


def parse_manifest(data: str) -> dict[str, str]:
    m: dict[str, str] = {}
    for line in data.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        if "=" not in line:
            raise RuntimeError("MALFORMED_MANIFEST")
        k, v = line.split("=", 1)
        if not k or k in m:
            raise RuntimeError("DUPLICATE_OR_EMPTY_MANIFEST_KEY")
        m[k] = v
    return m


def require(kv: dict[str, str], key: str, value: str):
    if kv.get(key) != value:
        raise RuntimeError(f"MANIFEST_MISMATCH_{key}: {kv.get(key)!r} != {value!r}")


def validate_zip(path: Path) -> tuple[str, int]:
    with zipfile.ZipFile(path) as z:
        infos = z.infolist()
        names = [i.filename for i in infos]
        if len(names) != len(set(names)):
            raise RuntimeError("DUPLICATE_ZIP_ENTRIES")
        for name in names:
            parts = name.split("/")
            if name.startswith("/") or "\\" in name or ".." in parts:
                raise RuntimeError(f"UNSAFE_ZIP_ENTRY_{name}")
        actual = set(names)
        symlinks = z.read("SYMLINKS.txt").decode("utf-8")
        for line in symlinks.splitlines():
            if not line:
                continue
            parts = line.split("←")
            if len(parts) != 2:
                raise RuntimeError("MALFORMED_SYMLINKS")
            link = parts[1].removeprefix("./")
            if not link or link.startswith("/") or ".." in link.split("/"):
                raise RuntimeError("UNSAFE_SYMLINK")
            actual.add(link)
        missing = REQUIRED - actual
        if missing:
            raise RuntimeError("MISSING_RUNTIME_ENTRIES_" + ",".join(sorted(missing)))
        profile = json.loads(z.read("BOOTSTRAP_PROFILE.json"))
        if any(profile.get(k) != v for k, v in {
            "arch": "arm", "package_name": PACKAGE, "prefix": PREFIX,
            "profile": "real-pkg", "package_layer": "real-pkg",
        }.items()):
            raise RuntimeError("PROFILE_NOT_REAL_ARM32")
        info = parse_manifest(z.read("BOOTSTRAP_INFO").decode("utf-8"))
        require(info, "TERMUX_ARCH", "arm")
        require(info, "TERMUX_PACKAGE_NAME", PACKAGE)
        for name in ELFS:
            blob = z.read(name)
            if len(blob) < 20 or blob[:4] != b"\x7fELF" or blob[4] != 1 or blob[5] != 1 or int.from_bytes(blob[18:20], "little") != 40:
                raise RuntimeError(f"INVALID_ARM32_ELF_{name}")
            if b"/data/data/com.termux/files/usr" in blob:
                raise RuntimeError(f"LEGACY_PREFIX_ELF_{name}")
        pkg = z.read("bin/pkg")
        if b"RAFCODEPHI pkg bridge" in pkg or b"real apt/apt-get backend is not installed yet" in pkg:
            raise RuntimeError("BRIDGE_PKG_NOT_REAL")
        status = z.read("var/lib/dpkg/status").decode("utf-8")
        stanzas = [set(x.splitlines()) for x in status.split("\n\n")]
        if not any({"Package: ninja", "Status: install ok installed"} <= s for s in stanzas):
            raise RuntimeError("NINJA_NOT_INSTALLED")
        source = z.read("etc/apt/sources.list.d/termux.sources").decode("utf-8")
        if "Enabled: no" not in source or "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED" not in source:
            raise RuntimeError("UNEXPECTED_APT_STATE")
        if z.testzip() is not None:
            raise RuntimeError("ZIP_CRC_FAILURE")
        return sha256(path), path.stat().st_size


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--download-dir", type=Path, required=True)
    ap.add_argument("--custody-dir", type=Path, required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    ap.add_argument("--source-run", type=int, required=True)
    ap.add_argument("--source-sha", required=True)
    ap.add_argument("--max-bytes", type=int, default=85_000_000)
    ns = ap.parse_args()
    if ns.source_run <= 0 or ns.max_bytes <= 0 or not re.fullmatch("[0-9a-f]{40}", ns.source_sha):
        raise SystemExit("INVALID_SOURCE_OR_CAP")
    source = exactly_one(ns.download_dir, "rafcodephi-bootstrap-arm.zip")
    original_manifest = exactly_one(ns.download_dir, "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt")
    custody = exactly_one(ns.custody_dir, "rafcodephi-bootstrap-matrix-custody.tsv")
    evidence = exactly_one(ns.download_dir, "rafcodephi-bootstrap-evidence.tar.gz")
    evidence_sha_file = exactly_one(ns.download_dir, "rafcodephi-bootstrap-evidence.tar.gz.sha256")
    manifest_text = original_manifest.read_text(encoding="utf-8")
    m = parse_manifest(manifest_text)
    for k, v in {
        "schema": SCHEMA,
        "package_name": PACKAGE,
        "prefix": PREFIX,
        "bridge_allowed": "false",
        "legacy_prefix_allowed": "false",
        "prefix_gate_state_arm": "PASS_SCOPED",
        "deb_gate_state_arm": "PASS_SCOPED",
        "claim_allowed_device_runtime": "false",
        "device_runtime_proof": "TOKEN_VAZIO",
        "apt_update_guard": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
    }.items():
        require(m, k, v)
    if not str(m.get("bytes_arm", "")).isdigit() or int(m["bytes_arm"]) != source.stat().st_size:
        raise RuntimeError("ZIP_SIZE_MISMATCH_WITH_PRODUCER_MANIFEST")
    rows = list(csv.DictReader(custody.open(encoding="utf-8", newline=""), delimiter="\t"))
    matched = [row for row in rows if row.get("arch") == "arm"]
    if len(matched) != 1 or matched[0].get("source_commit") != ns.source_sha or matched[0].get("claim_allowed") != "false":
        raise RuntimeError("CUSTODY_SOURCE_SHA_MISMATCH")
    recorded_evidence_sha = matched[0].get("sha256", "")
    declared_evidence_sha = evidence_sha_file.read_text().split()[0]
    if recorded_evidence_sha != declared_evidence_sha or sha256(evidence) != recorded_evidence_sha:
        raise RuntimeError("EVIDENCE_TAR_SHA256_MISMATCH")
    zip_hash, bytes_count = validate_zip(source)
    if bytes_count > ns.max_bytes:
        raise RuntimeError(f"ARM32_ZIP_ABOVE_CAP_{bytes_count}_MAX_{ns.max_bytes}")
    ns.output_dir.mkdir(parents=True, exist_ok=True)
    dest = ns.output_dir / "rafcodephi-bootstrap-arm.zip"
    shutil.copyfile(source, dest)
    if sha256(dest) != zip_hash:
        raise RuntimeError("COPY_SHA256_MISMATCH")
    if "sha256_arm" in m:
        raise RuntimeError("PRODUCER_MANIFEST_ALREADY_DECLARED_SHA256_ARM")
    new_manifest = ns.output_dir / "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
    new_manifest.write_text(manifest_text.rstrip("\n") + f"\nsha256_arm={zip_hash}\n", encoding="utf-8")
    receipt = {
        "schema": "rafcodephi.arm32-compact-handoff/v1",
        "source_run_id": ns.source_run,
        "source_commit": ns.source_sha,
        "source_arm_artifact": f"rafcodephi-real-bootstrap-{ns.source_run}-arm",
        "source_custody_artifact": f"rafcodephi-real-bootstrap-{ns.source_run}-custody",
        "source_evidence_tar_sha256": recorded_evidence_sha,
        "source_manifest_sha256": sha256(original_manifest),
        "zip_sha256": zip_hash,
        "bytes": bytes_count,
        "target_bytes_max": ns.max_bytes,
        "architecture": "arm", "android_abi": "armeabi-v7a",
        "package_name": PACKAGE, "prefix": PREFIX,
        "zip_payload_unchanged": True,
        "apt_update": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
        "validation": "PASS_SCOPED_STATIC_ARCHIVE",
        "device_runtime": "TOKEN_VAZIO",
        "claim_allowed": False,
    }
    (ns.output_dir / "ARM32_COMPACT_RECEIPT.json").write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    (ns.output_dir / "SHA256SUMS.txt").write_text(
        f"{zip_hash}  rafcodephi-bootstrap-arm.zip\n"
        f"{sha256(new_manifest)}  RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt\n", encoding="utf-8")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
