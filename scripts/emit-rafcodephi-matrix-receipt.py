#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

SCHEMA = "rafcodephi-source-pkg-handoff-matrix/v1"
EXPECTED = ("aarch64", "arm")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()

    reports = []
    errors = []
    for arch in EXPECTED:
        base = args.root / arch
        receipt_path = base / "handoff.v1.json"
        if not receipt_path.exists():
            errors.append(f"MISSING_HANDOFF:{arch}")
            continue
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        tar_path = base / receipt.get("bootstrap_tar", "")
        if receipt.get("arch") != arch:
            errors.append(f"ARCH_MISMATCH:{arch}")
        if receipt.get("state") != "BUILT_STRUCTURAL":
            errors.append(f"STATE_NOT_BUILT:{arch}:{receipt.get('state')}")
        if receipt.get("legacy_prefix_hits"):
            errors.append(f"LEGACY_PREFIX_HITS:{arch}")
        if not tar_path.is_file():
            errors.append(f"MISSING_TAR:{arch}")
        elif sha256(tar_path) != receipt.get("bootstrap_tar_sha256"):
            errors.append(f"TAR_HASH_MISMATCH:{arch}")
        reports.append(
            {
                "arch": arch,
                "handoff": str(receipt_path),
                "handoff_sha256": sha256(receipt_path),
                "bootstrap_tar": str(tar_path),
                "bootstrap_tar_sha256": receipt.get("bootstrap_tar_sha256"),
                "package_count": len(receipt.get("packages", [])),
            }
        )

    payload = {
        "schema": SCHEMA,
        "state": "PASS" if not errors and len(reports) == len(EXPECTED) else "FAIL",
        "architectures": list(EXPECTED),
        "reports": reports,
        "errors": errors,
        "claim_allowed_source_matrix": not errors and len(reports) == len(EXPECTED),
        "claim_allowed_device_runtime": False,
        "claim_allowed_pkg_runtime": False,
        "device_validation": "TOKEN_VAZIO",
        "next_required_action": "IMPORT_MATRIX_IN_TERMUX_APP_AND_RUN_DEVICE_SMOKE",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, sort_keys=True))
    return 0 if payload["state"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
