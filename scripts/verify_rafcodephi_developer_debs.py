#!/usr/bin/env python3
"""Verify the static developer-package handoff without claiming Android runtime."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REQUIRED_PATHS = {
    "nodejs-lts": ("bin/node",),
    "openjdk-21": (
        "lib/jvm/java-21-openjdk/bin/java",
        "lib/jvm/java-21-openjdk/bin/javac",
    ),
    "openjdk-21-runtime-image": ("lib/jvm/java-21-openjdk/lib/modules",),
    "android-tools": ("bin/adb", "bin/fastboot"),
    "clang": ("bin/clang",),
    "make": ("bin/make",),
    "ninja": ("bin/ninja",),
    "ndk-sysroot": ("include/android/api-level.h",),
    "rafcodephi-developer-profile": (
        "bin/rafcodephi-dev-doctor",
        "etc/profile.d/rafcodephi-developer.sh",
        "share/rafcodephi/profiles/developer.profile",
    ),
}
PROFILE_DEPENDENCIES = {
    "nodejs-lts", "npm", "openjdk-21", "android-tools",
    "clang", "make", "ninja", "ndk-sysroot",
}
RECOMMENDED_CLOSURE = {"npm", "ca-certificates-java", "openjdk-21-x", "resolv-conf"}


class VerificationError(RuntimeError):
    pass


def run(*args: str) -> str:
    proc = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, check=False)
    if proc.returncode:
        raise VerificationError(f"command failed ({proc.returncode}): {' '.join(args)}: {proc.stderr[-400:]}")
    return proc.stdout


def field(deb: Path, name: str) -> str:
    return run("dpkg-deb", "-f", str(deb), name).strip()


def package_names(value: str) -> set[str]:
    names: set[str] = set()
    for group in value.split(","):
        for alternative in group.split("|"):
            token = re.sub(r"\s*\([^)]*\)\s*", "", alternative).strip()
            token = token.split(":", 1)[0].strip()
            if token:
                names.add(token)
    return names


def paths(deb: Path) -> set[str]:
    producer = subprocess.Popen(
        ["dpkg-deb", "--fsys-tarfile", str(deb)], stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert producer.stdout is not None
    consumer = subprocess.run(
        ["tar", "-tf", "-"], stdin=producer.stdout, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    producer.stdout.close()
    producer_stderr = producer.stderr.read().decode(errors="replace") if producer.stderr else ""
    producer_status = producer.wait()
    if producer_status or consumer.returncode:
        raise VerificationError(
            f"cannot list {deb.name}: dpkg={producer_status} tar={consumer.returncode} "
            f"{producer_stderr[-200:]} {consumer.stderr[-200:]}"
        )
    return {line.removeprefix("./").rstrip("/") for line in consumer.stdout.splitlines()}


def has_suffix(entries: set[str], suffix: str) -> bool:
    return any(entry == suffix or entry.endswith("/" + suffix) for entry in entries)


def verify(deb_dir: Path, arch: str, source_commit: str) -> dict:
    if arch not in {"arm", "aarch64"}:
        raise VerificationError(f"unsupported architecture: {arch}")
    packages: dict[str, dict] = {}
    for deb in sorted(deb_dir.glob("*.deb")):
        name = field(deb, "Package")
        if name in packages:
            raise VerificationError(f"duplicate package output: {name}")
        package_arch = field(deb, "Architecture")
        if package_arch not in {arch, "all"}:
            raise VerificationError(f"wrong architecture: {name}={package_arch}, expected {arch}|all")
        packages[name] = {
            "path": deb,
            "architecture": package_arch,
            "version": field(deb, "Version"),
            "sha256": hashlib.sha256(deb.read_bytes()).hexdigest(),
            "bytes": deb.stat().st_size,
        }

    missing = (set(REQUIRED_PATHS) | RECOMMENDED_CLOSURE) - set(packages)
    if missing:
        raise VerificationError("missing package outputs: " + ",".join(sorted(missing)))

    checked_paths: dict[str, list[str]] = {}
    for name, required in REQUIRED_PATHS.items():
        entries = paths(packages[name]["path"])
        absent = [item for item in required if not has_suffix(entries, item)]
        if absent:
            raise VerificationError(f"{name} missing payload paths: {','.join(absent)}")
        checked_paths[name] = list(required)

    profile_deb = packages["rafcodephi-developer-profile"]["path"]
    profile_deps = package_names(field(profile_deb, "Depends"))
    if not PROFILE_DEPENDENCIES <= profile_deps:
        raise VerificationError(
            "developer profile dependency closure missing: " +
            ",".join(sorted(PROFILE_DEPENDENCIES - profile_deps))
        )
    openjdk_deps = package_names(field(packages["openjdk-21"]["path"], "Depends"))
    if "openjdk-21-runtime-image" not in openjdk_deps:
        raise VerificationError("openjdk-21 does not depend on split runtime image")

    oversize = [name for name, item in packages.items() if item["bytes"] >= 100 * 1024 * 1024]
    if oversize:
        raise VerificationError("APT object exceeds GitHub 100 MiB limit: " + ",".join(sorted(oversize)))

    selected = {}
    for name in sorted(set(REQUIRED_PATHS) | RECOMMENDED_CLOSURE):
        item = packages[name]
        selected[name] = {key: value for key, value in item.items() if key != "path"}
    return {
        "schema": "rafcodephi.developer-package-static-gate/v1",
        "state": "PASS_STATIC_DEB_CONTENT",
        "source_commit": source_commit,
        "architecture": arch,
        "profile": "rafcodephi-developer-profile",
        "packages": selected,
        "checked_paths": checked_paths,
        "shell_initialization": "PROFILE_D_STATIC_CONTENT_PRESENT",
        "ndk_scope": "TERMUX_ON_DEVICE_SYSROOT_NOT_GOOGLE_HOST_NDK",
        "physical_android_runtime": "TOKEN_VAZIO",
        "claim_allowed": False,
    }


def self_test() -> None:
    parsed = package_names("nodejs-lts (>= 24), npm | yarn, clang:any")
    if parsed != {"nodejs-lts", "npm", "yarn", "clang"}:
        raise VerificationError(f"dependency parser self-test failed: {parsed}")
    if not has_suffix({"data/data/pkg/files/usr/bin/node"}, "bin/node"):
        raise VerificationError("path suffix self-test failed")
    print("RAFCODEPHI_DEVELOPER_DEB_GATE_SELF_TEST=PASS")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--deb-dir", type=Path)
    parser.add_argument("--arch", choices=("arm", "aarch64"))
    parser.add_argument("--source-commit", default="TOKEN_VAZIO_REF")
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
            return 0
        if not args.deb_dir or not args.arch or not args.receipt:
            parser.error("--deb-dir, --arch and --receipt are required")
        result = verify(args.deb_dir.resolve(), args.arch, args.source_commit)
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"RAFCODEPHI_DEVELOPER_DEB_GATE=PASS_STATIC arch={args.arch} receipt={args.receipt}")
        return 0
    except (OSError, VerificationError, ValueError) as exc:
        print(f"RAFCODEPHI_DEVELOPER_DEB_GATE=BLOCKED reason={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
