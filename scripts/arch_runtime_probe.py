#!/usr/bin/env python3
"""Evidence-first runtime architecture capability probe.

This probe intentionally does not reuse the nominal architecture matrix as
runtime evidence. Every reported capability is either observed from the current
process/kernel environment or represented as TOKEN_VAZIO with a source/reason.
"""
from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import sys
import time
from pathlib import Path
from typing import Any

SCHEMA = "arch_runtime_probe/1.0.0"
TOKEN = "TOKEN_VAZIO"


def observed(value: Any, source: str) -> dict[str, Any]:
    return {"state": "OBSERVED", "value": value, "source": source}


def gap(reason: str, source: str | None = None) -> dict[str, Any]:
    item: dict[str, Any] = {"state": TOKEN, "value": None, "reason": reason}
    if source:
        item["source"] = source
    return item


def normalize_machine(machine: str, system: str) -> str:
    m = machine.lower()
    s = system.lower()
    if m in {"x86_64", "amd64"}:
        return "x86_64"
    if m in {"i386", "i486", "i586", "i686", "x86"}:
        return "i386"
    if m in {"aarch64", "arm64"}:
        return "arm64_darwin" if s == "darwin" else "arm64"
    if m.startswith("armv") or m in {"arm", "armhf", "armel"}:
        return "arm32"
    if m == "riscv64":
        return "riscv64"
    if m == "riscv32":
        return "riscv32"
    if m.startswith("mips64"):
        return "mips64"
    if m.startswith("mips"):
        return "mips32"
    if m in {"ppc64le", "powerpc64le"}:
        return "ppc64le"
    if m in {"ppc", "powerpc"}:
        return "ppc32"
    if m == "s390x":
        return "s390x"
    if m == "sparc64":
        return "sparc64"
    if m == "loongarch64":
        return "loongarch64"
    if m in {"wasm32", "wasm"}:
        return "wasm32"
    return "unknown"


def read_cpu_flags() -> tuple[set[str], str | None]:
    path = Path("/proc/cpuinfo")
    if not path.is_file():
        return set(), None
    flags: set[str] = set()
    try:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            if key.strip().lower() in {"flags", "features", "isa"}:
                flags.update(token.lower() for token in value.split())
    except OSError:
        return set(), None
    return flags, str(path)


def probe_page_size() -> dict[str, Any]:
    try:
        value = os.sysconf("SC_PAGESIZE")
    except (AttributeError, OSError, ValueError):
        return gap("os.sysconf(SC_PAGESIZE) unavailable or failed")
    if isinstance(value, int) and value > 0:
        return observed(value, "os.sysconf(SC_PAGESIZE)")
    return gap("SC_PAGESIZE returned non-positive/non-integer value")


def probe_cache_line() -> dict[str, Any]:
    candidates = [
        Path("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size"),
        Path("/sys/devices/system/cpu/cpu0/cache/index1/coherency_line_size"),
    ]
    for path in candidates:
        try:
            if path.is_file():
                value = int(path.read_text(encoding="utf-8").strip())
                if value > 0:
                    return observed(value, str(path))
        except (OSError, ValueError):
            continue
    return gap("cache-line sysfs value not observable", "linux sysfs cache topology")


def probe_simd(flags: set[str], source: str | None) -> dict[str, Any]:
    if source is None:
        return gap("CPU flags/features source unavailable", "/proc/cpuinfo")

    evidence = {
        "mmx": "mmx" in flags,
        "sse2": "sse2" in flags,
        "avx2": "avx2" in flags,
        "avx512f": "avx512f" in flags,
        "neon": "neon" in flags or "asimd" in flags,
        "sve": "sve" in flags,
        "rvv": any(f == "v" or f.startswith("rvv") for f in flags),
        "altivec": "altivec" in flags or "vmx" in flags,
    }
    return observed(evidence, source)


def probe_emulators() -> dict[str, Any]:
    names = {
        "x86_64": "qemu-x86_64",
        "i386": "qemu-i386",
        "arm64": "qemu-aarch64",
        "arm32": "qemu-arm",
        "riscv64": "qemu-riscv64",
        "riscv32": "qemu-riscv32",
        "mips64": "qemu-mips64",
        "mips32": "qemu-mips",
        "ppc64le": "qemu-ppc64le",
        "ppc32": "qemu-ppc",
        "s390x": "qemu-s390x",
        "sparc64": "qemu-sparc64",
        "loongarch64": "qemu-loongarch64",
    }
    values = {}
    for arch, executable in names.items():
        path = shutil.which(executable)
        values[arch] = {
            "available": path is not None,
            "executable": executable,
            "path": path,
        }
    return observed(values, "PATH via shutil.which")


def build_report() -> dict[str, Any]:
    raw_machine = platform.machine() or ""
    system = platform.system() or ""
    release = platform.release() or ""
    flags, flags_source = read_cpu_flags()
    normalized = normalize_machine(raw_machine, system)

    return {
        "schema": SCHEMA,
        "status": "OBSERVED_LIMITED",
        "claim_allowed": False,
        "generated_unix_ms": int(time.time() * 1000),
        "identity": {
            "machine_raw": observed(raw_machine, "platform.machine()") if raw_machine else gap("platform.machine() empty"),
            "arch_normalized": observed(normalized, "normalized platform.machine()/platform.system()") if normalized != "unknown" else gap(f"unrecognized machine: {raw_machine!r}"),
            "system": observed(system, "platform.system()") if system else gap("platform.system() empty"),
            "release": observed(release, "platform.release()") if release else gap("platform.release() empty"),
        },
        "capabilities": {
            "page_size_bytes": probe_page_size(),
            "cache_line_bytes": probe_cache_line(),
            "simd_features": probe_simd(flags, flags_source),
            "emulators": probe_emulators(),
        },
        "device": gap("no physical-device identity/proof is inferred from architecture probing"),
        "scope": {
            "nominal_arch_matrix_is_runtime_evidence": False,
            "physical_device_verified": False,
            "cross_arch_execution_verified": False,
            "security_claim": False,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    report = build_report()
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.out:
        args.out.write_text(text, encoding="utf-8")
        print(f"ARCH_RUNTIME_PROBE=OBSERVED_LIMITED out={args.out}")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
