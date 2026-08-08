#!/usr/bin/env python3
"""Emit a cryptographic source manifest for a RAFCODEPHI real-pkg bootstrap ZIP.

This proves artifact identity + compatible embedded profile. It does not prove
DEB repository reachability, APK installation, physical runtime, or performance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

SCHEMA = "rafcodephi.bootstrap-source-manifest/v1"
PROFILE_SCHEMA = "rafcodephi-bootstrap-profile/v1"
PACKAGE = "com.termux.rafacodephi"
PREFIX = f"/data/data/{PACKAGE}/files/usr"
REPO = "rafaelmeloreisnovo/termux-packages"
ARCH_TO_ABI = {"arm": "armeabi-v7a", "aarch64": "arm64-v8a"}
HEX = set("0123456789abcdef")


class ManifestError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def is_hex(value: str, size: int) -> bool:
    return len(value) == size and all(c in HEX for c in value)


def safe_zip(path: Path) -> dict:
    seen: set[str] = set()
    with zipfile.ZipFile(path) as zf:
        for info in zf.infolist():
            name = info.filename
            p = PurePosixPath(name)
            if not name or name.startswith("/") or "\\" in name or any(part in ("", ".", "..") for part in p.parts):
                raise ManifestError(f"unsafe ZIP entry: {name!r}")
            if name in seen:
                raise ManifestError(f"duplicate ZIP entry: {name}")
            seen.add(name)
        corrupt = zf.testzip()
        if corrupt is not None:
            raise ManifestError(f"ZIP CRC failure at {corrupt}")
        try:
            raw = zf.read("BOOTSTRAP_PROFILE.json")
        except KeyError as exc:
            raise ManifestError("BOOTSTRAP_PROFILE.json missing") from exc

    if len(raw) > 65536:
        raise ManifestError("BOOTSTRAP_PROFILE.json exceeds 64 KiB")
    try:
        profile = json.loads(raw)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ManifestError("BOOTSTRAP_PROFILE.json invalid") from exc
    if not isinstance(profile, dict):
        raise ManifestError("BOOTSTRAP_PROFILE.json must be an object")
    return profile


def validate_profile(profile: dict, arch: str) -> None:
    expected = {
        "schema": PROFILE_SCHEMA,
        "profile": "real-pkg",
        "package_layer": "real-pkg",
        "package_name": PACKAGE,
        "prefix": PREFIX,
        "arch": arch,
        "legacy_prefix_forbidden": True,
        "bridge_markers_forbidden": True,
        "claim_allowed": False,
        "release_allowed": False,
    }
    drift = [key for key, value in expected.items() if profile.get(key) != value]
    if drift:
        raise ManifestError("bootstrap profile drift: " + ", ".join(drift))
    if profile.get("device_validation") != "TOKEN_VAZIO":
        raise ManifestError("source bootstrap must not claim device validation")


def resolve_git_commit(explicit: str | None) -> str:
    value = explicit
    if not value:
        try:
            value = subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True, stderr=subprocess.DEVNULL
            ).strip()
        except (OSError, subprocess.CalledProcessError) as exc:
            raise ManifestError("--git-commit required outside a Git checkout") from exc
    if not is_hex(value, 40):
        raise ManifestError("git commit must be lowercase 40-hex")
    return value


def emit(artifact: Path, arch: str, git_commit: str) -> dict:
    if arch not in ARCH_TO_ABI:
        raise ManifestError(f"unsupported arch: {arch}")
    if not artifact.is_file():
        raise ManifestError(f"artifact missing: {artifact}")
    profile = safe_zip(artifact)
    validate_profile(profile, arch)

    profile_canonical = json.dumps(profile, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()
    return {
        "schema": SCHEMA,
        "repository": REPO,
        "git_commit": git_commit,
        "artifact_name": artifact.name,
        "artifact_size": artifact.stat().st_size,
        "artifact_sha256": sha256_file(artifact),
        "bootstrap_profile_sha256": hashlib.sha256(profile_canonical).hexdigest(),
        "arch": arch,
        "android_abi": ARCH_TO_ABI[arch],
        "package": PACKAGE,
        "prefix": PREFIX,
        "profile": "real-pkg",
        "artifact_gate": "PASS",
        "device_runtime": "TOKEN_VAZIO",
        "claim_allowed": False,
    }


def self_test() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        artifact = root / "bootstrap-aarch64.zip"
        profile = {
            "schema": PROFILE_SCHEMA,
            "profile": "real-pkg",
            "package_layer": "real-pkg",
            "package_name": PACKAGE,
            "prefix": PREFIX,
            "arch": "aarch64",
            "legacy_prefix_forbidden": True,
            "bridge_markers_forbidden": True,
            "device_validation": "TOKEN_VAZIO",
            "claim_allowed": False,
            "release_allowed": False,
        }
        with zipfile.ZipFile(artifact, "w") as zf:
            zf.writestr("BOOTSTRAP_PROFILE.json", json.dumps(profile))
            zf.writestr("BOOTSTRAP_INFO", "test=1\n")
        out = emit(artifact, "aarch64", "a" * 40)
        assert out["artifact_gate"] == "PASS"
        assert out["android_abi"] == "arm64-v8a"
        assert out["claim_allowed"] is False

        profile["profile"] = "bridge"
        bad = root / "bridge.zip"
        with zipfile.ZipFile(bad, "w") as zf:
            zf.writestr("BOOTSTRAP_PROFILE.json", json.dumps(profile))
        try:
            emit(bad, "aarch64", "a" * 40)
        except ManifestError:
            pass
        else:
            raise AssertionError("bridge profile must be rejected")
    print("SELF_TEST PASS: real-pkg=PASS bridge=REJECTED device_runtime=TOKEN_VAZIO")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--artifact")
    ap.add_argument("--arch", choices=sorted(ARCH_TO_ABI))
    ap.add_argument("--git-commit")
    ap.add_argument("--out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if not args.artifact or not args.arch or not args.out:
        ap.error("--artifact, --arch and --out are required")

    try:
        doc = emit(Path(args.artifact), args.arch, resolve_git_commit(args.git_commit))
    except ManifestError as exc:
        print(f"BLOCKED: {exc}")
        return 2

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"BOOTSTRAP_SOURCE_MANIFEST=PASS path={out} sha256={sha256_file(out)}")
    print("DEVICE_RUNTIME=TOKEN_VAZIO")
    print("CLAIM_ALLOWED=false")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
