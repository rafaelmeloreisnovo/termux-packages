#!/usr/bin/env python3
"""Static contract for the source-build bootstrap GitHub workflow."""
from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-real-bootstrap.yml"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_REAL_BOOTSTRAP_WORKFLOW=BLOCKED reason={token}")


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")
    require("scripts/apply-rafcodephi-build-properties.py" in text, "CANONICAL_PROPERTIES_OVERLAY_MISSING")
    require("scripts/validate-rafcodephi-build-properties.sh" in text, "PROPERTIES_VALIDATION_MISSING")
    require("path.write_text" not in text, "INLINE_PROPERTIES_MUTATION_PRESENT")
    require("rafcodephi-real-bootstrap.log" in text, "BUILD_LOG_CAPTURE_MISSING")
    require("rafcodephi-real-bootstrap-status.json" in text, "BUILD_STATUS_RECEIPT_MISSING")
    require("build_status=\"${PIPESTATUS[0]}\"" in text, "DOCKER_EXIT_STATUS_LOST")
    require("RAFCODEPHI_REAL_BOOTSTRAP_BUILD=BLOCKED" in text, "FAIL_CLOSED_STATUS_MISSING")
    require("- name: Upload real bootstrap artifacts\n        if: always()" in text,
            "FAILURE_ARTIFACT_NOT_ALWAYS_UPLOADED")
    builder = (ROOT / "scripts/build-rafcodephi-real-bootstrap.sh").read_text(encoding="utf-8")
    generator = (ROOT / "scripts/generate-bootstraps.sh").read_text(encoding="utf-8")
    require('"runtime_materialized": False' in builder,
            "ARCHIVE_MATERIALIZATION_STATE_MISSING")
    require('TERMUX_BOOTSTRAP_OUTPUT_DIR' in generator,
            "WRITABLE_BOOTSTRAP_OUTPUT_HOOK_MISSING")
    require('BOOTSTRAP_OUTPUT_DIR="${TERMUX_BOOTSTRAP_OUTPUT_DIR:-.}"' in generator,
            "UPSTREAM_DEFAULT_OUTPUT_COMPATIBILITY_MISSING")
    require('"$BOOTSTRAP_OUTPUT_DIR/"' in generator,
            "BOOTSTRAP_ARCHIVE_NOT_ROUTED_TO_OUTPUT_DIR")
    require('export TERMUX_BOOTSTRAP_OUTPUT_DIR="$GENERATED_BOOTSTRAP_DIR"' in builder,
            "RAFCODEPHI_WRITABLE_GENERATED_OUTPUT_NOT_BOUND")
    require('zip_path="$GENERATED_BOOTSTRAP_DIR/bootstrap-${arch}.zip"' in builder,
            "RAFCODEPHI_GENERATED_ZIP_ROUTE_MISSING")
    print("RAFCODEPHI_REAL_BOOTSTRAP_WORKFLOW=PASS canonical_overlay=true failure_evidence=true writable_archive_output=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
