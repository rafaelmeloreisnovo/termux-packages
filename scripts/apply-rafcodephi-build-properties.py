#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

TARGET_PACKAGE = "com.termux.rafacodephi"
UPSTREAM_PACKAGE = "com.termux"
CURRENT_ASSIGNMENT = 'TERMUX_APP__PACKAGE_NAME="com.termux"'
TARGET_ASSIGNMENT = 'TERMUX_APP__PACKAGE_NAME="com.termux.rafacodephi"'
REPO_ASSIGNMENT = 'TERMUX_REPO_APP__PACKAGE_NAME="com.termux"'
LEGACY_PREFIX = "/data/data/com.termux/files/usr"
TARGET_PREFIX = "/data/data/com.termux.rafacodephi/files/usr"
SCHEMA = "rafcodephi-build-properties-overlay/v1"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def audit_source(text: str) -> dict:
    current_count = text.count(CURRENT_ASSIGNMENT)
    target_count = text.count(TARGET_ASSIGNMENT)
    repo_count = text.count(REPO_ASSIGNMENT)
    return {
        "current_assignment_count": current_count,
        "target_assignment_count": target_count,
        "repo_assignment_count": repo_count,
    }


def transform(text: str) -> tuple[str, dict]:
    before = audit_source(text)

    # Idempotent operation is allowed only if the target is already exact and
    # the official repository identity is still preserved.
    if before["target_assignment_count"] == 1 and before["current_assignment_count"] == 0:
        if before["repo_assignment_count"] != 1:
            raise RuntimeError("REPO_IDENTITY_CONTRACT_INVALID")
        return text, {"mode": "ALREADY_APPLIED", **before}

    if before["current_assignment_count"] != 1:
        raise RuntimeError(
            "APP_PACKAGE_ASSIGNMENT_NOT_UNIQUE: "
            f"expected=1 observed={before['current_assignment_count']}"
        )
    if before["target_assignment_count"] != 0:
        raise RuntimeError(
            "TARGET_ASSIGNMENT_ALREADY_PRESENT_WITH_UPSTREAM_ASSIGNMENT: "
            f"observed={before['target_assignment_count']}"
        )
    if before["repo_assignment_count"] != 1:
        raise RuntimeError(
            "REPO_IDENTITY_CONTRACT_INVALID: "
            f"expected=1 observed={before['repo_assignment_count']}"
        )

    transformed = text.replace(CURRENT_ASSIGNMENT, TARGET_ASSIGNMENT, 1)
    after = audit_source(transformed)
    if after != {
        "current_assignment_count": 0,
        "target_assignment_count": 1,
        "repo_assignment_count": 1,
    }:
        raise RuntimeError(f"POST_TRANSFORM_CONTRACT_INVALID: {after}")
    return transformed, {"mode": "APPLIED", **after}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--properties", type=Path, default=Path("scripts/properties.sh"))
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()

    source = args.properties
    receipt = args.receipt
    raw_before = source.read_bytes()
    text_before = raw_before.decode("utf-8")

    try:
        text_after, transform_result = transform(text_before)
        raw_after = text_after.encode("utf-8")
        state = "PASS"
        reason = "RAFCODEPHI_APP_PACKAGE_BUILD_PROPERTY_BOUND"
    except Exception as exc:
        payload = {
            "schema": SCHEMA,
            "state": "FAIL",
            "reason": str(exc),
            "properties": str(source),
            "sha256_before": sha256_bytes(raw_before),
            "claim_allowed": False,
            "release_allowed": False,
        }
        receipt.parent.mkdir(parents=True, exist_ok=True)
        receipt.write_text(json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(payload, sort_keys=True))
        return 1

    if not args.check_only and raw_after != raw_before:
        source.write_bytes(raw_after)

    payload = {
        "schema": SCHEMA,
        "state": state,
        "reason": reason,
        "mode": transform_result["mode"],
        "properties": str(source),
        "sha256_before": sha256_bytes(raw_before),
        "sha256_after": sha256_bytes(raw_after),
        "changed": raw_after != raw_before,
        "target_package": TARGET_PACKAGE,
        "target_prefix": TARGET_PREFIX,
        "repository_package_identity": UPSTREAM_PACKAGE,
        "repository_prefix": LEGACY_PREFIX,
        "repository_identity_preserved": True,
        "binary_rewrite_performed": False,
        "claim_allowed": False,
        "release_allowed": False,
        "next_required_action": "SOURCE_PROPERTIES_RUNTIME_ASSERTION",
    }
    receipt.parent.mkdir(parents=True, exist_ok=True)
    receipt.write_text(json.dumps(payload, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
