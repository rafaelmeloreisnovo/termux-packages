#!/usr/bin/env python3
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "termux-build-core"
GENERATOR = ROOT / "manifest_generator.py"
MAGIC = 0x5445524D
VERSION = 1
ENTRY_SIZE = 184
HEADER_SIZE = 20


def run(*args):
    return subprocess.run(
        args,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def write_manifest(path: Path, name="hello-world", version="2.12"):
    pool = b"\0"
    header = struct.pack(
        "<IIIII", MAGIC, VERSION, 1, 0, HEADER_SIZE + 4 + ENTRY_SIZE
    )
    entry = struct.pack(
        "<64s32sBBH8IHH4I16H",
        name.encode().ljust(64, b"\0"),
        version.encode().ljust(32, b"\0"),
        0,
        24,
        0,
        *([0] * 8),
        0,
        0,
        0,
        0,
        0,
        0,
        *([0] * 16),
    )
    path.write_bytes(header + struct.pack("<I", len(pool)) + entry + pool)


def assert_generator_layout(root: Path):
    packages = root / "packages"
    pkg = packages / "fixture"
    pkg.mkdir(parents=True)
    (pkg / "build.sh").write_text(
        "TERMUX_PKG_VERSION=1.0\n"
        "TERMUX_PKG_SRCURL=https://example.invalid/source.tar.gz\n"
        "TERMUX_PKG_SHA256=" + "0" * 64 + "\n",
        encoding="utf-8",
    )
    output = root / "generated.bin"
    generated = run("python3", str(GENERATOR), str(packages), str(output))
    assert generated.returncode == 0, generated.stdout
    raw = output.read_bytes()
    magic, version, count, _, pool_offset = struct.unpack("<IIIII", raw[:20])
    assert (magic, version, count) == (MAGIC, VERSION, 1)
    assert pool_offset == HEADER_SIZE + 4 + ENTRY_SIZE
    pool_size = struct.unpack("<I", raw[20:24])[0]
    assert raw[pool_offset] == 0
    assert pool_offset + pool_size == len(raw)


def main():
    with tempfile.TemporaryDirectory() as td_raw:
        td = Path(td_raw)
        assert_generator_layout(td)

        good = td / "good.bin"
        write_manifest(good)

        missing = run(str(CORE), "--manifest", str(good), "--package", "not-present")
        assert missing.returncode != 0 and "not found" in missing.stdout.lower(), missing.stdout

        malformed = td / "malformed.bin"
        malformed.write_bytes(b"TERM")
        bad = run(str(CORE), "--manifest", str(malformed), "--package", "hello-world")
        assert bad.returncode != 0, bad.stdout

        no_source = run(
            str(CORE),
            "--manifest",
            str(good),
            "--package",
            "hello-world",
            "--source-dir",
            str(td / "absent"),
            "--output",
            str(td / "out"),
        )
        assert no_source.returncode != 0
        assert "TOKEN_VAZIO_SOURCE_DIR_NOT_MATERIALIZED" in no_source.stdout, no_source.stdout

        resume = run(
            str(CORE),
            "--manifest",
            str(good),
            "--package",
            "hello-world",
            "--resume",
            str(td / "checkpoint"),
        )
        assert resume.returncode != 0
        assert "TOKEN_VAZIO_RESUME_NOT_VALIDATED" in resume.stdout

    print("MANIFEST_FAIL_CLOSED_PASS cases=5 claim_allowed=false")


if __name__ == "__main__":
    main()
