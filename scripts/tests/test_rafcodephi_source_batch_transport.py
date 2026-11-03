#!/usr/bin/env python3
"""Scoped synthetic transport test: Debian epoch filenames across Actions lots.

This verifies safe packaging/custody, not a real build, APT publication or device.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-source-batches.yml"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_BATCH_TRANSPORT=BLOCKED reason={token}")


def main() -> int:
    workflow = WORKFLOW.read_text(encoding="utf-8")
    for token in (
        "arch: [arm, aarch64]",
        "python3 scripts/tests/test_rafcodephi_source_batch_transport.py",
        'tar -C "$lot" -cf "$archive" .',
        'sha256sum -c "${arch}.tar.sha256"',
        'tar -xf "$arch.tar"',
        "rafcodephi-source-${{ github.run_id }}-${{ matrix.arch }}",
    ):
        require(token in workflow, f"WORKFLOW_TRANSPORT_CONTRACT_MISSING:{token}")
    require("path: build/rafcodephi-lots/${{ matrix.arch }}/" not in workflow,
            "RAW_DEBIAN_EPOCH_PATHS_STILL_UPLOADED")
    with tempfile.TemporaryDirectory(prefix="rafcodephi-lot-transport-") as tmp:
        root = Path(tmp)
        lot = root / "arm"
        deb = lot / "debs" / "ca-certificates-java_1:2026.05.14_all.deb"
        deb.parent.mkdir(parents=True)
        deb.write_bytes(b"synthetic-epoch-transport-not-a-real-deb")
        (lot / "build.state").write_text("arch=arm\nsource_step=success\n")
        archive = root / "arm.tar"
        subprocess.run(["tar", "-C", str(lot), "-cf", str(archive), "."], check=True)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        checksum = root / "arm.tar.sha256"
        checksum.write_text(f"{digest}  arm.tar\n", encoding="utf-8")
        subprocess.run(["sha256sum", "-c", checksum.name],
                       cwd=root, check=True, capture_output=True)
        handoff = root / "handoff"
        handoff.mkdir()
        subprocess.run(["tar", "-xf", str(archive), "-C", str(handoff)], check=True)
        restored = handoff / "debs" / deb.name
        require(restored.read_bytes() == deb.read_bytes(), "EPOCH_FILENAME_ROUNDTRIP_MISMATCH")
        with archive.open("ab") as file:
            file.write(b"tamper")
        invalid = subprocess.run(["sha256sum", "-c", checksum.name],
                                 cwd=root, capture_output=True)
        require(invalid.returncode != 0, "TAMPER_NOT_REJECTED")
    print("RAFCODEPHI_BATCH_TRANSPORT=PASS scope=HOST_SYNTHETIC epoch_colon=true sha256_tamper_rejected=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
