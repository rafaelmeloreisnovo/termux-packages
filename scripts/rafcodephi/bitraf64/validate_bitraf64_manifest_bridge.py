#!/usr/bin/env python3
"""Validate the experimental RAFCODEΦ BITRAF64 manifest bridge.

Structural validity is intentionally distinct from scientific/production PASS.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any

SCHEMA = "rafcodephi.bitraf64.manifest-bridge.v1"
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
RECEIPTS = ("rank_receipt", "ecc_receipt", "benchmark_receipt")


def fail(message: str) -> int:
    print(json.dumps({"valid": False, "state": "INVALIDATED", "claim_allowed": False, "error": message}, ensure_ascii=False, sort_keys=True))
    return 1


def nonempty_ref(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def validate(record: dict[str, Any]) -> tuple[bool, dict[str, Any]]:
    errors: list[str] = []
    if record.get("schema") != SCHEMA:
        errors.append(f"schema must be {SCHEMA}")
    package = record.get("package")
    if not isinstance(package, str) or not package.strip():
        errors.append("package must be a non-empty string")
    artifact_sha256 = record.get("artifact_sha256")
    if not isinstance(artifact_sha256, str) or not SHA256_RE.fullmatch(artifact_sha256):
        errors.append("artifact_sha256 must be 64 lowercase hex characters")

    bitraf = record.get("bitraf64")
    if not isinstance(bitraf, dict):
        errors.append("bitraf64 must be an object")
        bitraf = {}

    state = bitraf.get("state", "TOKEN_VAZIO")
    claim_allowed = bitraf.get("claim_allowed", False)
    if not isinstance(claim_allowed, bool):
        errors.append("bitraf64.claim_allowed must be boolean")
        claim_allowed = False

    coordinate = bitraf.get("coordinate")
    if coordinate is not None:
        if not (isinstance(coordinate, list) and len(coordinate) == 4 and all(isinstance(x, int) and not isinstance(x, bool) for x in coordinate)):
            errors.append("coordinate must be null or integer [i,j,k,f]")
        else:
            i, j, k, f = coordinate
            if not (0 <= i <= 9 and 0 <= j <= 9 and 0 <= k <= 9 and 0 <= f <= 5):
                errors.append("coordinate domain is i,j,k in 0..9 and f in 0..5")

    receipts = {name: bitraf.get(name) for name in RECEIPTS}
    complete_receipts = all(nonempty_ref(v) for v in receipts.values())

    # Fail closed: promotion cannot be asserted without every declared evidence class.
    if claim_allowed and not complete_receipts:
        errors.append("claim_allowed=true requires rank_receipt, ecc_receipt and benchmark_receipt")
    if claim_allowed and state in {"TOKEN_VAZIO", "BLOCKED", "NOT_MEASURED", "UNAVAILABLE", "INVALIDATED", "OBSERVED_LIMITED"}:
        errors.append(f"claim_allowed=true contradicts state={state}")

    promotion_blocked = not claim_allowed or not complete_receipts
    result = {
        "valid": not errors,
        "schema": SCHEMA,
        "state": state,
        "claim_allowed": claim_allowed if not errors else False,
        "production_ready": False,
        "promotion_blocked": True if errors else promotion_blocked,
        "coordinate_validated_as_address_only": coordinate is not None and not any("coordinate" in e for e in errors),
        "receipt_presence": {k: nonempty_ref(v) for k, v in receipts.items()},
        "invariants": {
            "gcd_6000_2057": 1,
            "package_dag_toroidal_alignment_proven": False,
            "sha256_remains_independent_integrity_anchor": True,
        },
        "errors": errors,
    }
    return not errors, result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--strict", action="store_true", help="exit 2 while promotion remains blocked")
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()

    try:
        raw = args.manifest.read_bytes()
    except OSError as exc:
        return fail(f"cannot read manifest: {exc}")

    try:
        record = json.loads(raw)
    except json.JSONDecodeError as exc:
        return fail(f"invalid JSON: {exc}")
    if not isinstance(record, dict):
        return fail("top-level JSON must be an object")

    valid, result = validate(record)
    result["manifest_sha256"] = hashlib.sha256(raw).hexdigest()
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    if not valid:
        return 1
    if args.strict and result["promotion_blocked"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
