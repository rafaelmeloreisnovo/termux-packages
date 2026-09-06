#!/usr/bin/env python3
"""Emit a source-built RAFCODEPHI local apt repository + bootstrap TAR.

Producer boundary:
- consumes only .deb files produced by the RAFCODEPHI package-name/prefix build;
- rejects legacy com.termux prefix bytes in the materialized payload;
- creates an offline file: apt repository inside the bootstrap prefix;
- emits uncompressed USTAR with symlinks last for the freestanding RAF-NINJA extractor;
- never claims Android/device/pkg runtime success.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

PACKAGE_NAME = "com.termux.rafacodephi"
PREFIX = f"/data/data/{PACKAGE_NAME}/files/usr"
PREFIX_REL = Path("data") / "data" / PACKAGE_NAME / "files" / "usr"
LEGACY = b"/data/data/com.termux/files/usr"
SCHEMA = "rafcodephi-source-pkg-handoff/v1"
REQUIRED = (
    "bin/sh",
    "bin/bash",
    "bin/apt",
    "bin/apt-get",
    "bin/dpkg",
    "bin/pkg",
    "bin/proot",
    "bin/proot.real",
    "etc/apt/sources.list",
    "var/lib/dpkg/status",
)


def sha256_path(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_control(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    current: str | None = None
    for raw in text.splitlines():
        if raw.startswith((" ", "\t")) and current:
            fields[current] += "\n" + raw
            continue
        if ":" not in raw:
            continue
        key, value = raw.split(":", 1)
        current = key
        fields[key] = value.lstrip()
    return fields


def control_for(deb: Path) -> tuple[str, dict[str, str]]:
    proc = subprocess.run(
        ["dpkg-deb", "-f", str(deb)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return proc.stdout, parse_control(proc.stdout)


def package_paragraph(deb: Path, pool_name: str, raw_control: str) -> str:
    raw = raw_control.rstrip("\n")
    return (
        raw
        + f"\nFilename: pool/{pool_name}"
        + f"\nSize: {deb.stat().st_size}"
        + f"\nSHA256: {sha256_path(deb)}\n"
    )


def release_text(arch: str, packages_rel: str, packages_file: Path) -> str:
    digest = sha256_path(packages_file)
    size = packages_file.stat().st_size
    return (
        "Origin: RAFCODEPHI\n"
        "Label: RAFCODEPHI Source-Built Local Repository\n"
        "Suite: stable\n"
        "Codename: stable\n"
        f"Architectures: {arch}\n"
        "Components: main\n"
        "Description: Hash-bound local package repository for RAFCODEPHI device validation\n"
        "SHA256:\n"
        f" {digest} {size} {packages_rel}\n"
    )


def merge_debs(debs: list[Path], rootfs: Path) -> None:
    for deb in debs:
        subprocess.run(
            ["dpkg-deb", "-x", str(deb), str(rootfs)],
            check=True,
            stdout=subprocess.DEVNULL,
        )


def ensure_dirs(prefix: Path) -> None:
    for rel in (
        "etc/apt/apt.conf.d",
        "var/lib/dpkg",
        "var/lib/apt/lists/partial",
        "var/cache/apt/archives/partial",
        "var/lib/rafcodephi/repo",
        "tmp",
    ):
        (prefix / rel).mkdir(parents=True, exist_ok=True)
    status = prefix / "var/lib/dpkg/status"
    if not status.exists():
        status.write_bytes(b"")


def configure_local_repo(prefix: Path, repo: Path) -> None:
    repo_dst = prefix / "var/lib/rafcodephi/repo"
    if repo_dst.exists():
        shutil.rmtree(repo_dst)
    shutil.copytree(repo, repo_dst, symlinks=True)

    sources = prefix / "etc/apt/sources.list"
    sources.parent.mkdir(parents=True, exist_ok=True)
    sources.write_text(
        f"deb [trusted=yes] file:{PREFIX}/var/lib/rafcodephi/repo stable main\n",
        encoding="utf-8",
    )
    (prefix / "etc/rafcodephi-core.env").write_text(
        "RAFCODEPHI_SOURCE_BUILT_CORE=1\n"
        f"TERMUX_PACKAGE_NAME={PACKAGE_NAME}\n"
        f"TERMUX_PREFIX={PREFIX}\n"
        "RAFCODEPHI_REPOSITORY_MODE=LOCAL_HASH_BOUND\n",
        encoding="utf-8",
    )


def ensure_proot_real(prefix: Path) -> None:
    proot = prefix / "bin/proot"
    proot_real = prefix / "bin/proot.real"
    if not proot.exists():
        raise RuntimeError("SOURCE_BUILD_MISSING_PROOT")
    if not proot_real.exists():
        if proot.is_symlink():
            target = os.readlink(proot)
            proot_real.symlink_to(target)
        else:
            shutil.copy2(proot, proot_real)


def scan_legacy_prefix(prefix: Path) -> list[str]:
    hits: list[str] = []
    for path in sorted(prefix.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as f:
            carry = b""
            while True:
                chunk = f.read(1024 * 1024)
                if not chunk:
                    break
                data = carry + chunk
                if LEGACY in data:
                    hits.append(path.relative_to(prefix).as_posix())
                    break
                carry = data[-(len(LEGACY) - 1) :] if len(data) >= len(LEGACY) else data
    return hits


def validate_required(prefix: Path) -> None:
    missing = [rel for rel in REQUIRED if not (prefix / rel).exists() and not (prefix / rel).is_symlink()]
    if missing:
        raise RuntimeError("MISSING_REQUIRED_PREFIX_ENTRIES:" + ",".join(missing))


def add_tar_member(tf: tarfile.TarFile, path: Path, prefix: Path) -> None:
    rel = path.relative_to(prefix).as_posix()
    if rel.startswith("/") or ".." in rel.split("/"):
        raise RuntimeError(f"UNSAFE_TAR_PATH:{rel}")
    info = tf.gettarinfo(str(path), arcname=rel)
    # Deterministic metadata. Preserve executable and symlink semantics, not host identity/time.
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mtime = 0
    if path.is_file() and not path.is_symlink():
        with path.open("rb") as f:
            tf.addfile(info, f)
    else:
        tf.addfile(info)


def make_bootstrap_tar(prefix: Path, tar_path: Path) -> None:
    tar_path.parent.mkdir(parents=True, exist_ok=True)
    paths = sorted(prefix.rglob("*"), key=lambda p: p.relative_to(prefix).as_posix())
    normal = [p for p in paths if not p.is_symlink()]
    links = [p for p in paths if p.is_symlink()]
    with tarfile.open(tar_path, "w", format=tarfile.USTAR_FORMAT) as tf:
        for path in normal:
            add_tar_member(tf, path, prefix)
        # RAF-NINJA's extractor requires symlinks in the tail so subsequent
        # regular-file writes can never traverse a newly created symlink.
        for path in links:
            add_tar_member(tf, path, prefix)


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", required=True, choices=("aarch64", "arm"))
    ap.add_argument("--packages-dir", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()

    debs = sorted(args.packages_dir.glob("*.deb"))
    if not debs:
        raise SystemExit("NO_DEB_ARTIFACTS")

    args.output.mkdir(parents=True, exist_ok=True)
    repo = args.output / "repo"
    pool = repo / "pool"
    index_dir = repo / "dists/stable/main" / f"binary-{args.arch}"
    pool.mkdir(parents=True, exist_ok=True)
    index_dir.mkdir(parents=True, exist_ok=True)

    package_records: list[dict] = []
    paragraphs: list[str] = []
    for deb in debs:
        raw_control, fields = control_for(deb)
        pkg = fields.get("Package", "")
        version = fields.get("Version", "")
        arch = fields.get("Architecture", "")
        if not pkg or not version or arch not in (args.arch, "all"):
            raise RuntimeError(f"INVALID_DEB_CONTROL:{deb.name}:package={pkg}:version={version}:arch={arch}")
        pool_name = deb.name
        dst = pool / pool_name
        shutil.copy2(deb, dst)
        paragraphs.append(package_paragraph(dst, pool_name, raw_control))
        package_records.append(
            {
                "package": pkg,
                "version": version,
                "architecture": arch,
                "filename": f"pool/{pool_name}",
                "size": dst.stat().st_size,
                "sha256": sha256_path(dst),
            }
        )

    packages_file = index_dir / "Packages"
    packages_file.write_text("\n".join(paragraphs) + "\n", encoding="utf-8")
    packages_rel = f"main/binary-{args.arch}/Packages"
    (repo / "dists/stable/Release").write_text(
        release_text(args.arch, packages_rel, packages_file), encoding="utf-8"
    )

    with tempfile.TemporaryDirectory(prefix=f"rafcodephi-prefix-{args.arch}-") as tmp_s:
        rootfs = Path(tmp_s) / "rootfs"
        rootfs.mkdir()
        merge_debs(debs, rootfs)
        prefix = rootfs / PREFIX_REL
        if not prefix.exists():
            raise RuntimeError(f"SOURCE_BUILD_PREFIX_MISSING:{prefix}")
        ensure_dirs(prefix)
        ensure_proot_real(prefix)
        configure_local_repo(prefix, repo)
        validate_required(prefix)
        legacy_hits = scan_legacy_prefix(prefix)
        if legacy_hits:
            raise RuntimeError("LEGACY_PREFIX_BINARY_OR_TEXT_RISK:" + ",".join(legacy_hits[:50]))

        tar_path = args.output / f"rafcodephi-bootstrap-{args.arch}.tar"
        make_bootstrap_tar(prefix, tar_path)
        tar_sha = sha256_path(tar_path)

    producer_commit = subprocess.run(
        ["git", "rev-parse", "HEAD"], check=True, stdout=subprocess.PIPE, text=True
    ).stdout.strip()
    receipt = {
        "schema": SCHEMA,
        "state": "BUILT_STRUCTURAL",
        "producer_repository": "rafaelmeloreisnovo/termux-packages",
        "producer_commit": producer_commit,
        "arch": args.arch,
        "target_package": PACKAGE_NAME,
        "target_prefix": PREFIX,
        "source_built": True,
        "legacy_prefix_hits": [],
        "local_repository": "repo",
        "repository_mode": "LOCAL_HASH_BOUND_TRUSTED_FOR_DEVICE_TEST",
        "packages": sorted(package_records, key=lambda x: (x["package"], x["version"], x["filename"])),
        "bootstrap_tar": tar_path.name,
        "bootstrap_tar_sha256": tar_sha,
        "claim_allowed_source_build": True,
        "claim_allowed_install": False,
        "claim_allowed_pkg_runtime": False,
        "claim_allowed_device_runtime": False,
        "device_validation": "TOKEN_VAZIO",
        "next_required_action": "CONSUMER_IMPORT_THEN_DEVICE_REAL_PKG_SMOKE",
    }
    write_json(args.output / "handoff.v1.json", receipt)
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
