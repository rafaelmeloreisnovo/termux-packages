#!/usr/bin/env python3
"""Fail-closed scoped ELF gate for RAFCODEPHI Python on Android API 29.

Checks the actual staged libpython for known API30-only Bionic imports.
Does not claim complete API29 compatibility or physical device execution.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

LATE_SYMBOL_MIN_API = {"sem_clockwait": 30, "pthread_cond_clockwait": 30}
MACHINES = {"arm": ("ELF32", "ARM", 1), "aarch64": ("ELF64", "AArch64", 2)}
SYMBOL_LINE = re.compile(r"\bUND\s+([^\s]+)")


def undefined_symbols(dynsym: str) -> set[str]:
    result: set[str] = set()
    for line in dynsym.splitlines():
        if not re.match(r"^\s*\d+:", line):
            continue
        match = SYMBOL_LINE.search(line)
        if match:
            result.add(match.group(1).split("@", 1)[0])
    return result


def inspect(path: Path, arch: str, max_api: int) -> dict:
    out = {
        "schema": "rafcodephi.python-elf-api29/v1",
        "target_arch": arch,
        "max_android_api": max_api,
        "library": path.name,
        "sha256": None,
        "state": "BLOCKED",
        "reasons": [],
        "checked_symbols": sorted(LATE_SYMBOL_MIN_API),
        "device_execution": "TOKEN_VAZIO",
        "claim_allowed": False,
    }
    try:
        if not path.is_file():
            raise ValueError("LIBPYTHON_MISSING")
        if not 52 <= path.stat().st_size <= 64 * 1024 * 1024:
            raise ValueError("LIBPYTHON_SIZE_INVALID")
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            first = stream.read(5)
            if first[:4] != b"\x7fELF":
                raise ValueError("NOT_ELF")
            if first[4] != MACHINES[arch][2]:
                raise ValueError("ELF_CLASS_MISMATCH")
            digest.update(first)
            for block in iter(lambda: stream.read(1 << 20), b""):
                digest.update(block)
        out["sha256"] = digest.hexdigest()
        header = subprocess.run(
            ["readelf", "-W", "-h", str(path)],
            text=True, capture_output=True, check=False, timeout=20,
        )
        if header.returncode:
            raise ValueError("ELF_HEADER_UNREADABLE")
        elf_class, machine, _ = MACHINES[arch]
        if not re.search(r"^\s*Class:\s*" + elf_class + r"\s*$", header.stdout, re.M):
            raise ValueError("ELF_HEADER_CLASS_MISMATCH")
        if not re.search(r"^\s*Machine:\s*" + machine + r"\s*$", header.stdout, re.M):
            raise ValueError("ELF_MACHINE_MISMATCH")
        symbols = subprocess.run(
            ["readelf", "-W", "--dyn-syms", str(path)],
            text=True, capture_output=True, check=False, timeout=20,
        )
        if symbols.returncode or "Symbol table" not in symbols.stdout:
            raise ValueError("ELF_DYNSYM_UNREADABLE")
        imports = undefined_symbols(symbols.stdout)
        late = sorted(symbol for symbol, required_api in LATE_SYMBOL_MIN_API.items()
                      if symbol in imports and required_api > max_api)
        if late:
            out["reasons"].append("LATE_ANDROID_BIONIC_IMPORT:" + ",".join(late))
        else:
            out["state"] = "PASS_SCOPED_STATIC"
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        out["reasons"].append("ELF_INSPECTION_BLOCKED:" + str(exc)[:160])
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--library", type=Path, required=True)
    ap.add_argument("--arch", choices=sorted(MACHINES), required=True)
    ap.add_argument("--max-api", type=int, default=29)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()
    if not 21 <= args.max_api <= 35:
        ap.error("--max-api must be in 21..35")
    result = inspect(args.library, args.arch, args.max_api)
    serialized = json.dumps(result, sort_keys=True, indent=2) + "\n"
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(serialized, encoding="utf-8")
    print(serialized, end="")
    return 0 if result["state"] == "PASS_SCOPED_STATIC" else 2


if __name__ == "__main__":
    raise SystemExit(main())
