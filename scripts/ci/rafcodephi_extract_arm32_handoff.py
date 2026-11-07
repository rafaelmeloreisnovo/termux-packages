#!/usr/bin/env python3
"""Publish a byte-identical ARM32 Termux bootstrap from a producer artifact.

This is a scoped artifact handoff, not an APK, APT publication or phone test.
Nothing inside the original bootstrap is pruned.
"""
import argparse
import hashlib
import json
import shutil
import struct
import zipfile
from pathlib import Path

PACKAGE = "com.termux.rafacodephi"
PREFIX = "/data/data/com.termux.rafacodephi/files/usr"
MAX_INPUT = 85 * 1024 * 1024
REQUIRED = (
    "BOOTSTRAP_INFO", "BOOTSTRAP_PROFILE.json", "SYMLINKS.txt",
    "bin/bash", "bin/apt", "bin/apt-get", "bin/dpkg", "bin/proot",
    "bin/busybox", "bin/ninja", "var/lib/dpkg/status",
    "etc/apt/sources.list.d/termux.sources",
    "etc/apt/apt.conf.d/00rafcodephi-repository-block",
)

def fail(reason):
    raise SystemExit("BLOCKED_ARM32_HANDOFF: " + reason)

def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def unique(root, basename):
    found = [p for p in root.rglob(basename) if p.is_file() and p.name == basename]
    if len(found) != 1:
        fail(f"exactly one {basename} required; got {len(found)}")
    return found[0]

def kv(payload, label):
    try:
        decoded = payload.decode("utf-8")
    except UnicodeDecodeError:
        fail(label + ": invalid UTF-8")
    data = {}
    for row in decoded.splitlines():
        if not row or row.startswith("#"):
            continue
        if "=" not in row:
            fail(label + ": invalid key-value row")
        k, v = row.split("=", 1)
        if not k or k in data:
            fail(label + ": empty/duplicate key")
        data[k] = v
    return data

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--source-run", required=True)
    ap.add_argument("--source-sha", required=True)
    args = ap.parse_args()
    if not args.input.is_dir() or not (
        args.source_run.isdecimal()
        and len(args.source_sha) == 40
        and all(c in "0123456789abcdef" for c in args.source_sha)
    ):
        fail("invalid input directory/source identity")

    archive = unique(args.input, "rafcodephi-bootstrap-arm.zip")
    manifest = unique(args.input, "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt")
    size = archive.stat().st_size
    if size > MAX_INPUT:
        fail(f"ZIP size {size} exceeds 85 MiB without safe lossless reduction; no pruning permitted")
    if size < 1024:
        fail("input ZIP unusually small")

    manifest_bytes = manifest.read_bytes()
    m = kv(manifest_bytes, "producer manifest")
    for key, expected in {
        "schema": "rafcodephi.real-bootstrap-sourcebuild/v1",
        "package_name": PACKAGE,
        "prefix": PREFIX,
        "bridge_allowed": "false",
        "legacy_prefix_allowed": "false",
        "build_tools_contract": "rafcodephi.apt-bootstrap-ninja/v1",
        "bootstrap_ninja": "EMBEDDED",
        "prefix_gate_state_arm": "PASS_SCOPED",
        "deb_gate_state_arm": "PASS_SCOPED",
        "claim_allowed_device_runtime": "false",
        "device_runtime_proof": "TOKEN_VAZIO",
        "package_repo_runtime_state": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
        "apt_update_guard": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
    }.items():
        if m.get(key) != expected:
            fail(f"manifest {key} mismatch: {m.get(key)!r}")
    if m.get("bytes_arm") != str(size):
        fail("manifest bytes_arm does not match ZIP bytes")
    zhash = sha256(archive)
    if m.get("sha256_arm") not in (None, zhash):
        fail("existing manifest SHA256 differs from ZIP")

    with zipfile.ZipFile(archive) as z:
        seen = set()
        for item in z.infolist():
            name = item.filename
            if (
                not name or name.startswith("/") or "\\" in name
                or any(part in (".", "..", "") for part in name.rstrip("/").split("/"))
                or name in seen
            ):
                fail("unsafe or duplicate ZIP entry: " + repr(name))
            seen.add(name)
        links = set()
        for line in z.read("SYMLINKS.txt").decode("utf-8").splitlines():
            if not line:
                continue
            parts = line.split("←")
            if len(parts) != 2 or not parts[0] or not parts[1]:
                fail("malformed SYMLINKS.txt")
            dest = parts[1]
            while dest.startswith("./"):
                dest = dest[2:]
            if dest.startswith("/") or ".." in dest.split("/") or dest in links:
                fail("unsafe symlink destination")
            links.add(dest)
        for name in REQUIRED:
            if name not in seen and name not in links:
                fail("missing runtime entry: " + name)
        if "bin/sh" not in seen and "bin/sh" not in links:
            fail("no bin/sh")
        profile = json.loads(z.read("BOOTSTRAP_PROFILE.json"))
        for key, expected in {
            "arch": "arm", "profile": "real-pkg",
            "package_name": PACKAGE, "prefix": PREFIX,
        }.items():
            if profile.get(key) != expected:
                fail("profile " + key + " mismatch")
        info = kv(z.read("BOOTSTRAP_INFO"), "BOOTSTRAP_INFO")
        for key, expected in {
            "TERMUX_ARCH": "arm",
            "TERMUX_PACKAGE_NAME": PACKAGE,
            "RAFCODEPHI_APT_UPDATE_GUARD": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
        }.items():
            if info.get(key) != expected:
                fail("bootstrap info " + key + " mismatch")
        for name in (
            "bin/bash", "bin/apt", "bin/apt-get", "bin/dpkg",
            "bin/proot", "bin/busybox", "bin/ninja",
        ):
            b = z.read(name)
            if (
                len(b) < 20 or b[:4] != b"\x7fELF"
                or b[4] != 1 or struct.unpack_from("<H", b, 18)[0] != 40
            ):
                fail("ELF ARM32 mismatch: " + name)
            if b"/data/data/com.termux/files/usr" in b:
                fail("legacy prefix embedded in " + name)
        dpkg = z.read("var/lib/dpkg/status").decode("utf-8")
        if not any(
            "Package: ninja" in s.splitlines()
            and "Status: install ok installed" in s.splitlines()
            for s in dpkg.split("\n\n")
        ):
            fail("ninja is not registered as installed")
        if z.testzip() is not None:
            fail("archive CRC failure")

    args.output.mkdir(parents=True, exist_ok=True)
    outzip = args.output / "rafcodephi-bootstrap-arm.zip"
    shutil.copyfile(archive, outzip)
    if sha256(outzip) != zhash:
        fail("output ZIP changed during copy")
    (args.output / "PRODUCER_SOURCE_MANIFEST.txt").write_bytes(manifest_bytes)
    derived_manifest = manifest_bytes + (
        b"" if manifest_bytes.endswith(b"\n") else b"\n"
    )
    if "sha256_arm" not in m:
        derived_manifest += f"sha256_arm={zhash}\n".encode()
    (args.output / "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt").write_bytes(
        derived_manifest
    )
    receipt = {
        "schema": "rafcodephi.arm32-bootstrap-handoff/v1",
        "kind": "BYTE_IDENTICAL_BOOTSTRAP_ONLY",
        "source_run_id": args.source_run,
        "source_commit": args.source_sha,
        "architecture": "arm",
        "apk_abi": "armeabi-v7a",
        "package_name": PACKAGE,
        "bootstrap_filename": outzip.name,
        "bootstrap_bytes": size,
        "bootstrap_sha256": zhash,
        "producer_manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "consumer_manifest_sha256": hashlib.sha256(derived_manifest).hexdigest(),
        "size_cap_bytes": MAX_INPUT,
        "bytes_changed": False,
        "source_gates": "PASS_SCOPED",
        "apk_built": False,
        "signed_apt_repository": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
        "physical_android": "TOKEN_VAZIO",
        "claim_allowed": False,
    }
    (args.output / "HANDOFF_RECEIPT.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n"
    )
    (args.output / "SHA256SUMS.txt").write_text(
        "".join(
            f"{sha256(args.output / name)}  {name}\n"
            for name in (
                "rafcodephi-bootstrap-arm.zip",
                "PRODUCER_SOURCE_MANIFEST.txt",
                "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt",
                "HANDOFF_RECEIPT.json",
            )
        )
    )
    print(json.dumps(receipt, sort_keys=True))

if __name__ == "__main__":
    main()
