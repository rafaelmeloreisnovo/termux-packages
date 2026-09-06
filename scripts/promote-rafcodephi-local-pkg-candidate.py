#!/usr/bin/env python3
"""Promote a validated source-built bootstrap COPY to a local-pkg test candidate.

The original real-bootstrap remains unchanged as LKG.  This tool embeds a
hash-bound `file:` apt repository made only from RAFCODEPHI source-built .debs
and replaces `bin/pkg` with a bounded RAFCODEPHI adapter that explicitly opts
apt into that local repository while leaving the persistent upstream repository
fail-closed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

PACKAGE = "com.termux.rafacodephi"
PREFIX = f"/data/data/{PACKAGE}/files/usr"
SCHEMA = "rafcodephi.local-pkg-candidate/v1"
LEGACY = b"/data/data/com.termux/files/usr"
ARCH = {
    "arm": {"class": 1, "machine": 40},
    "aarch64": {"class": 2, "machine": 183},
}
LOCAL_STATE = "LOCAL_HASH_BOUND_TEST_CHANNEL"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_path(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def control(deb: Path) -> dict[str, str]:
    out = subprocess.run(
        ["dpkg-deb", "-f", str(deb)], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    ).stdout
    fields: dict[str, str] = {}
    current: str | None = None
    for raw in out.splitlines():
        if raw.startswith((" ", "\t")) and current:
            fields[current] += "\n" + raw
        elif ":" in raw:
            current, value = raw.split(":", 1)
            fields[current] = value.lstrip()
    return fields


def package_index_paragraph(deb: Path, fields: dict[str, str]) -> str:
    ordered = []
    for key in (
        "Package", "Version", "Architecture", "Maintainer", "Installed-Size",
        "Depends", "Pre-Depends", "Conflicts", "Breaks", "Replaces", "Provides",
        "Multi-Arch", "Description", "Homepage",
    ):
        value = fields.get(key)
        if value:
            ordered.append(f"{key}: {value}")
    ordered += [
        f"Filename: pool/{deb.name}",
        f"Size: {deb.stat().st_size}",
        f"SHA256: {sha256_path(deb)}",
    ]
    return "\n".join(ordered) + "\n"


def validate_elf(data: bytes, arch: str, label: str) -> None:
    spec = ARCH[arch]
    if len(data) < 20 or data[:4] != b"\x7fELF":
        raise RuntimeError(f"{label}:NOT_ELF")
    if data[4] != spec["class"] or data[5] != 1:
        raise RuntimeError(f"{label}:ELF_CLASS_ENDIAN_MISMATCH")
    machine = struct.unpack_from("<H", data, 18)[0]
    if machine != spec["machine"]:
        raise RuntimeError(f"{label}:ELF_MACHINE_MISMATCH:{machine}")
    if LEGACY in data:
        raise RuntimeError(f"{label}:LEGACY_PREFIX")


def pkg_wrapper() -> bytes:
    script = f'''#!{PREFIX}/bin/sh
# RAFCODEPHI local-pkg adapter v1 — bounded device test channel.
set -eu
PREFIX="${{PREFIX:-{PREFIX}}}"
APTGET="$PREFIX/bin/apt-get"
APT="$PREFIX/bin/apt"
LOCAL_LIST="$PREFIX/etc/apt/rafcodephi-local.list"
EMPTY_PARTS="$PREFIX/etc/apt/rafcodephi-local.conf.d"

apt_opts() {{
  "$APTGET" \
    -o "Dir::Etc::sourcelist=$LOCAL_LIST" \
    -o "Dir::Etc::sourceparts=-" \
    -o "Dir::Etc::parts=$EMPTY_PARTS" \
    -o "Acquire::AllowInsecureRepositories=true" \
    -o "APT::Get::AllowUnauthenticated=true" \
    "$@"
}}

cmd="${{1:-help}}"
[ "$#" -eq 0 ] || shift
case "$cmd" in
  help|-h|--help)
    cat <<'EOF'
RAFCODEPHI pkg local test channel
  pkg update [-y]
  pkg install [-y] PACKAGE...
  pkg upgrade [-y]
  pkg remove [-y] PACKAGE...
  pkg show PACKAGE...
  pkg search TERM...
Persistent external repository remains fail-closed; this adapter uses only the
hash-bound repository embedded under $PREFIX/var/lib/rafcodephi/repo.
EOF
    ;;
  update)  apt_opts update "$@" ;;
  install) apt_opts install "$@" ;;
  upgrade) apt_opts upgrade "$@" ;;
  remove|uninstall) apt_opts remove "$@" ;;
  show) "$APT" -o "Dir::Etc::sourcelist=$LOCAL_LIST" -o "Dir::Etc::sourceparts=-" -o "Dir::Etc::parts=$EMPTY_PARTS" show "$@" ;;
  search) "$APT" -o "Dir::Etc::sourcelist=$LOCAL_LIST" -o "Dir::Etc::sourceparts=-" -o "Dir::Etc::parts=$EMPTY_PARTS" search "$@" ;;
  *)
    echo "RAFCODEPHI pkg: unsupported command '$cmd' in bounded local test channel" >&2
    exit 64
    ;;
esac
'''
    return script.encode()


def make_repo(debs: list[Path], arch: str) -> tuple[dict[str, bytes], list[dict]]:
    entries: dict[str, bytes] = {}
    paras: list[str] = []
    receipt_pkgs: list[dict] = []
    for deb in debs:
        fields = control(deb)
        pkg_arch = fields.get("Architecture", "")
        if pkg_arch not in (arch, "all"):
            continue
        package = fields.get("Package", "")
        version = fields.get("Version", "")
        if not package or not version:
            raise RuntimeError(f"BAD_CONTROL:{deb.name}")
        data = deb.read_bytes()
        rel = f"var/lib/rafcodephi/repo/pool/{deb.name}"
        entries[rel] = data
        paras.append(package_index_paragraph(deb, fields))
        receipt_pkgs.append({
            "package": package,
            "version": version,
            "architecture": pkg_arch,
            "filename": f"pool/{deb.name}",
            "sha256": sha256_bytes(data),
            "bytes": len(data),
        })
    if not receipt_pkgs:
        raise RuntimeError(f"NO_PACKAGES_FOR_ARCH:{arch}")
    packages = ("\n".join(paras) + "\n").encode()
    packages_rel = f"dists/stable/main/binary-{arch}/Packages"
    entries[f"var/lib/rafcodephi/repo/{packages_rel}"] = packages
    release = (
        "Origin: RAFCODEPHI\n"
        "Label: RAFCODEPHI Local Device Test\n"
        "Suite: stable\n"
        "Codename: stable\n"
        f"Architectures: {arch}\n"
        "Components: main\n"
        "Description: embedded hash-bound source-built package channel\n"
        "SHA256:\n"
        f" {sha256_bytes(packages)} {len(packages)} main/binary-{arch}/Packages\n"
    ).encode()
    entries["var/lib/rafcodephi/repo/dists/stable/Release"] = release
    return entries, sorted(receipt_pkgs, key=lambda x: (x["package"], x["version"], x["filename"]))


def clone_info(info: zipfile.ZipInfo) -> zipfile.ZipInfo:
    out = zipfile.ZipInfo(info.filename, info.date_time)
    out.compress_type = info.compress_type
    out.comment = info.comment
    out.extra = info.extra
    out.internal_attr = info.internal_attr
    out.external_attr = info.external_attr
    out.create_system = info.create_system
    out.flag_bits = info.flag_bits
    return out


def deterministic_info(name: str, mode: int = 0o100600) -> zipfile.ZipInfo:
    zi = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
    zi.create_system = 3
    zi.external_attr = mode << 16
    zi.compress_type = zipfile.ZIP_DEFLATED
    return zi


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--arch", required=True, choices=tuple(ARCH))
    ap.add_argument("--source-zip", required=True, type=Path)
    ap.add_argument("--packages-dir", required=True, type=Path)
    ap.add_argument("--output-zip", required=True, type=Path)
    ap.add_argument("--receipt", required=True, type=Path)
    args = ap.parse_args()

    debs = sorted(args.packages_dir.glob("*.deb"))
    repo_entries, package_receipts = make_repo(debs, args.arch)
    package_names = {x["package"] for x in package_receipts}
    for required_pkg in ("apt", "dpkg", "proot", "termux-tools", "nano", "python", "git"):
        if required_pkg not in package_names:
            raise RuntimeError(f"LOCAL_REPO_MISSING_REQUIRED_PACKAGE:{required_pkg}")

    with zipfile.ZipFile(args.source_zip, "r") as src:
        if src.testzip() is not None:
            raise RuntimeError("SOURCE_ZIP_CRC_FAIL")
        names = src.namelist()
        if len(names) != len(set(names)):
            raise RuntimeError("SOURCE_ZIP_DUPLICATE_ENTRIES")
        required = ("bin/pkg", "bin/apt", "bin/apt-get", "bin/dpkg", "bin/proot", "BOOTSTRAP_PROFILE.json", "BOOTSTRAP_INFO")
        for name in required:
            if name not in names:
                raise RuntimeError(f"SOURCE_ZIP_MISSING:{name}")
        for name in ("bin/apt", "bin/apt-get", "bin/dpkg", "bin/proot"):
            validate_elf(src.read(name), args.arch, name)

        profile = json.loads(src.read("BOOTSTRAP_PROFILE.json").decode())
        if profile.get("profile") != "real-pkg" or profile.get("package_name") != PACKAGE or profile.get("arch") != args.arch:
            raise RuntimeError("SOURCE_PROFILE_IDENTITY_MISMATCH")
        if profile.get("claim_allowed") is not False or profile.get("device_validation") != "TOKEN_VAZIO":
            raise RuntimeError("SOURCE_PROFILE_CLAIM_BOUNDARY_MISMATCH")
        profile["local_pkg_test_channel"] = LOCAL_STATE
        profile["local_pkg_repository"] = f"file:{PREFIX}/var/lib/rafcodephi/repo"
        profile["claim_allowed"] = False
        profile["release_allowed"] = False
        profile["device_validation"] = "TOKEN_VAZIO"

        info_lines = src.read("BOOTSTRAP_INFO").decode().splitlines()
        info = {}
        for line in info_lines:
            if "=" in line:
                k, v = line.split("=", 1)
                info[k] = v
        info["RAFCODEPHI_LOCAL_PKG_CHANNEL"] = "1"
        info["RAFCODEPHI_LOCAL_PKG_STATE"] = LOCAL_STATE
        info["RAFCODEPHI_LOCAL_PKG_DEVICE_VALIDATION"] = "TOKEN_VAZIO"

        local_list = (
            f"deb [trusted=yes] file:{PREFIX}/var/lib/rafcodephi/repo stable main\n"
        ).encode()
        repo_entries["etc/apt/rafcodephi-local.list"] = local_list
        repo_entries["etc/apt/rafcodephi-local.conf.d/.keep"] = b"RAFCODEPHI\n"

        original_pkg = src.read("bin/pkg")
        repo_entries["bin/pkg.termux-original"] = original_pkg
        repo_entries["bin/pkg"] = pkg_wrapper()
        if "bin/proot.real" not in names:
            repo_entries["bin/proot.real"] = src.read("bin/proot")

        args.output_zip.parent.mkdir(parents=True, exist_ok=True)
        fd, tmp_s = tempfile.mkstemp(prefix=args.output_zip.name + ".", suffix=".tmp", dir=args.output_zip.parent)
        os.close(fd)
        tmp = Path(tmp_s)
        try:
            replace = set(repo_entries) | {"BOOTSTRAP_PROFILE.json", "BOOTSTRAP_INFO"}
            with zipfile.ZipFile(tmp, "w", allowZip64=True) as out:
                for old in src.infolist():
                    if old.filename in replace:
                        continue
                    out.writestr(clone_info(old), src.read(old.filename))
                for name, payload in sorted(repo_entries.items()):
                    mode = 0o100700 if name in ("bin/pkg", "bin/pkg.termux-original", "bin/proot.real") else 0o100600
                    out.writestr(deterministic_info(name, mode), payload)
                out.writestr(deterministic_info("BOOTSTRAP_PROFILE.json"), (json.dumps(profile, sort_keys=True, separators=(",", ":")) + "\n").encode())
                out.writestr(deterministic_info("BOOTSTRAP_INFO"), "".join(f"{k}={info[k]}\n" for k in sorted(info)).encode())
            with zipfile.ZipFile(tmp, "r") as verify:
                if verify.testzip() is not None:
                    raise RuntimeError("CANDIDATE_ZIP_CRC_FAIL")
                validate_elf(verify.read("bin/proot.real"), args.arch, "bin/proot.real")
                if LEGACY in verify.read("bin/pkg"):
                    raise RuntimeError("PKG_WRAPPER_LEGACY_PREFIX")
            os.chmod(tmp, 0o644)
            os.replace(tmp, args.output_zip)
        finally:
            if tmp.exists():
                tmp.unlink()

    receipt = {
        "schema": SCHEMA,
        "state": "STRUCTURAL_CANDIDATE",
        "arch": args.arch,
        "package_name": PACKAGE,
        "prefix": PREFIX,
        "source_zip": str(args.source_zip),
        "source_zip_sha256": sha256_path(args.source_zip),
        "candidate_zip": str(args.output_zip),
        "candidate_zip_sha256": sha256_path(args.output_zip),
        "repository_mode": LOCAL_STATE,
        "repository_package_count": len(package_receipts),
        "repository_packages": package_receipts,
        "persistent_external_repo_state": "FAIL_CLOSED_UNCHANGED",
        "pkg_adapter": "RAFCODEPHI_LOCAL_BOUNDED_V1",
        "proot_real_preserved": True,
        "claim_allowed_install": False,
        "claim_allowed_pkg_runtime": False,
        "claim_allowed_device_runtime": False,
        "device_validation": "TOKEN_VAZIO",
        "next_required_action": "APP_IMPORT_AND_DEVICE_REAL_PKG_SMOKE",
    }
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
