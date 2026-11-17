#!/usr/bin/env python3
"""Inspect pure-object ABI + symbol closure; never execute the input ELF.

Not a full ELF linker/verifier, syscall disassembler, device or speed test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ABIS = {"armeabi-v7a": (1, 40, 52, 40), "arm64-v8a": (2, 183, 64, 64)}


def verify(path, abi, symbol):
    if not 52 <= path.stat().st_size <= 64 * 1024 * 1024:
        raise ValueError("ELF size outside budget")
    data = path.read_bytes()
    cls, machine, header, section_width = ABIS[abi]
    if data[:7] != b"\x7fELF" + bytes((cls, 1, 1)):
        raise ValueError("ELF ident/ABI mismatch")
    if len(data) < header:
        raise ValueError("truncated ELF header")
    kind, actual_machine, version = struct.unpack_from("<HHI", data, 16)
    if (kind, actual_machine, version) != (1, machine, 1):
        raise ValueError("requires ET_REL for exact target ABI")
    if cls == 1:
        sections = struct.unpack_from("<I", data, 32)[0]
        flags = struct.unpack_from("<I", data, 36)[0]
        sizes = struct.unpack_from("<HHHHHH", data, 40)
        if flags & 0xFF000000 != 0x05000000 or flags & 0x400:
            raise ValueError("requires ARM EABI5 softfp calling convention")
    else:
        sections = struct.unpack_from("<Q", data, 40)[0]
        sizes = struct.unpack_from("<HHHHHH", data, 52)
    ehsize, _, phnum, shentsize, shnum, shstr = sizes
    if (ehsize != header or phnum != 0 or shentsize != section_width or
            shnum == 0 or shstr >= shnum or sections < header or
            sections + shnum * shentsize > len(data)):
        raise ValueError("ELF section/header bounds")
    result = subprocess.run(["readelf", "--wide", "--symbols", str(path.resolve())],
                            check=True, capture_output=True, text=True, timeout=15)
    defined = False
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[0].endswith(":"):
            if fields[6] == "UND":
                raise ValueError("undefined external symbol: " + fields[7])
            if fields[7] == symbol and fields[3] == "FUNC" and fields[4] == "GLOBAL":
                defined = True
    if not defined:
        raise ValueError("required global function missing")
    return {"schema": "rafcodephi.pure-elf-object/v1", "artifact": path.name,
            "abi": abi, "sha256": hashlib.sha256(data).hexdigest(),
            "symbol": symbol, "undefined_symbols": 0, "status": "PASS_SCOPED",
            "scope": "ELF_RELOCATABLE_ABI_AND_SYMBOL_CLOSURE",
            "device_runtime": "TOKEN_VAZIO_NOT_RUN", "claim_allowed": False}


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("artifact", type=Path)
    p.add_argument("--abi", required=True, choices=ABIS)
    p.add_argument("--symbol", required=True)
    a = p.parse_args()
    print(json.dumps(verify(a.artifact, a.abi, a.symbol), indent=2))
