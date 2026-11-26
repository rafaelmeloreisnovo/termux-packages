#!/usr/bin/env python3
"""Fail-closed source-bootstrap Android API audit; reads bytes, never rewrites ELF.

Designed for source-built RAFCODEPHI packages, *before* ZIP packaging for APK.
PASS_SCOPED_STATIC is NOT a successful Android physical-device execution.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

PREFIX = "/data/data/com.termux.rafacodephi/files/usr"
REQUIRED_ELF = (
    "bin/bash", "bin/dash", "bin/apt", "bin/apt-get", "bin/dpkg",
    "bin/busybox", "bin/proot", "bin/ninja", "libexec/termux-api-broadcast",
)
REQUIRED_SHEBANG = {
    "bin/pkg": f"#!{PREFIX}/bin/bash",
    "bin/login": f"#!{PREFIX}/bin/sh",
}
# GNU readelf -n prints the note header and the first four API bytes on
# separate lines. Do not assume "Android ... description data:" on ONE line.
ANDROID_NOTE = re.compile(
    r"(?im)^[ \t]*Android[ \t]+0x[0-9a-f]+[ \t]+NT_VERSION[^\n]*\n"
    r"[ \t]*description data:[ \t]*((?:[0-9a-f]{2}[ \t]+){3}[0-9a-f]{2})"
)
ABI_ELF = {"arm": (1, 40), "aarch64": (2, 183)}


def parse_android_api(note: str) -> int | None:
    match = ANDROID_NOTE.search(note)
    return int.from_bytes(bytes.fromhex(match.group(1)), "little") if match else None


def check_elf(data: bytes, path: str, arch: str, max_api: int, readelf: str) -> dict:
    errors: list[str] = []
    level = None
    elf_class, machine = ABI_ELF[arch]
    if len(data) < 20 or data[:4] != b"\x7fELF":
        errors.append("NOT_ELF")
    elif data[4] != elf_class or data[5] != 1 or int.from_bytes(data[18:20], "little") != machine:
        errors.append("ABI_OR_ENDIANNESS_MISMATCH")
    else:
        try:
            with tempfile.TemporaryDirectory(prefix="rafcodephi-elf-") as temp:
                target = Path(temp) / "a.out"
                target.write_bytes(data)
                notes = subprocess.run(
                    [readelf, "-n", str(target)], capture_output=True,
                    text=True, timeout=15, check=False,
                )
                symbols = subprocess.run(
                    [readelf, "--dyn-syms", "--wide", str(target)],
                    capture_output=True, text=True, timeout=15, check=False,
                )
            if notes.returncode or symbols.returncode:
                errors.append("READELF_FAILED")
            else:
                level = parse_android_api(notes.stdout)
                if level is None:
                    errors.append("ANDROID_API_NOTE_MISSING")
                elif level > max_api:
                    errors.append("ANDROID_MIN_API_EXCEEDS_TARGET")
                if any(" UND " in line and re.search(r"\bmemfd_create(?:@|$)", line)
                       for line in symbols.stdout.splitlines()):
                    errors.append("UNRESOLVED_MEMFD_CREATE")
        except (OSError, subprocess.TimeoutExpired) as exc:
            errors.append("ELF_INSPECTION_ERROR:" + type(exc).__name__)
    return {"path": path, "api": level, "state": "BLOCKED" if errors else "PASS_SCOPED_STATIC",
            "reasons": errors}


def verify(archive: Path, arch: str, max_api: int, readelf: str | None = None) -> dict:
    result: dict = {
        "schema": "rafcodephi.source-bootstrap-android-api/v1",
        "archive": archive.name, "arch": arch, "target_api": max_api,
        "sha256": None, "state": "BLOCKED", "elf_checked": 0,
        "scripts_checked": 0, "issues": [], "api_distribution": {},
        "device_execution": "TOKEN_VAZIO", "claim_allowed": False,
        "release_allowed": False,
    }
    try:
        readelf = readelf or shutil.which("readelf") or shutil.which("llvm-readelf")
        if not readelf:
            raise ValueError("READELF_UNAVAILABLE")
        with archive.open("rb") as f:
            result["sha256"] = hashlib.file_digest(f, "sha256").hexdigest()
        with zipfile.ZipFile(archive) as zf:
            names = zf.namelist()
            if len(names) > 10000 or len(names) != len(set(names)):
                raise ValueError("ZIP_ENTRIES_BOUND_OR_DUPLICATE")
            if any(name.startswith("/") or ".." in Path(name).parts
                   for name in names):
                raise ValueError("ZIP_PATH_TRAVERSAL")
            members = set(names)
            for required in (*REQUIRED_ELF, *REQUIRED_SHEBANG, "SYMLINKS.txt"):
                if required not in members:
                    result["issues"].append({"path": required, "reasons": ["REQUIRED_MISSING"]})
            if "SYMLINKS.txt" in members:
                links = zf.read("SYMLINKS.txt").decode("utf-8", errors="replace")
                if not re.search(r"(?m)^dash←(?:\./)?bin/sh$", links):
                    result["issues"].append({"path": "bin/sh", "reasons": ["SH_SYMLINK_MISSING"]})
            for path in names:
                if path.endswith("/"):
                    continue
                entry = zf.getinfo(path)
                if entry.file_size > 32 * 1024 * 1024:
                    # A large non-ELF data file is okay; never decompress it just to detect ELF.
                    continue
                with zf.open(path) as stream:
                    lead = stream.read(4)
                if lead != b"\x7fELF" and path not in REQUIRED_SHEBANG:
                    continue
                data = zf.read(path)
                if lead == b"\x7fELF":
                    checked = check_elf(data, path, arch, max_api, readelf)
                    result["elf_checked"] += 1
                    key = str(checked["api"]) if checked["api"] is not None else "UNKNOWN"
                    result["api_distribution"][key] = result["api_distribution"].get(key, 0) + 1
                    if checked["state"] != "PASS_SCOPED_STATIC":
                        result["issues"].append({"path": path, "reasons": checked["reasons"],
                                                 "api": checked["api"]})
                else:
                    expected = REQUIRED_SHEBANG[path].encode("ascii")
                    result["scripts_checked"] += 1
                    if data.split(b"\n", 1)[0] != expected:
                        result["issues"].append({"path": path, "reasons": ["SHEBANG_MISMATCH"]})
            if result["elf_checked"] < len(REQUIRED_ELF):
                result["issues"].append({"path": "ELF_SET", "reasons": ["TOO_FEW_ELF_ENTRIES"]})
            for required in REQUIRED_ELF:
                if required in members:
                    with zf.open(required) as f:
                        if f.read(4) != b"\x7fELF":
                            result["issues"].append({"path": required,
                                                     "reasons": ["REQUIRED_NOT_ELF"]})
            if not result["issues"] and result["scripts_checked"] == len(REQUIRED_SHEBANG):
                result["state"] = "PASS_SCOPED_STATIC"
    except (OSError, ValueError, zipfile.BadZipFile, KeyError) as exc:
        result["issues"].append({"path": archive.name,
                                 "reasons": [type(exc).__name__ + ":" + str(exc)[:160]]})
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zip", type=Path, required=True)
    parser.add_argument("--arch", choices=sorted(ABI_ELF), required=True)
    parser.add_argument("--max-api", type=int, default=29)
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.max_api < 21 or args.max_api > 35:
        parser.error("--max-api outside supported Android range")
    report = verify(args.zip, args.arch, args.max_api)
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("RAFCODEPHI_BOOTSTRAP_API=" + report["state"],
          "arch=" + args.arch, "elfs=" + str(report["elf_checked"]),
          "issues=" + str(len(report["issues"])))
    for issue in report["issues"][:35]:
        print("  BLOCKED:", issue["path"], ",".join(issue["reasons"]))
    if len(report["issues"]) > 35:
        print("  ADDITIONAL_ISSUES=" + str(len(report["issues"]) - 35),
              "see_json_receipt=true")
    return 0 if report["state"] == "PASS_SCOPED_STATIC" else 1


if __name__ == "__main__":
    sys.exit(main())
