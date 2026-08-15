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
    require('"runtime_materialized": False' in (ROOT / "scripts/build-rafcodephi-real-bootstrap.sh").read_text(encoding="utf-8"),
            "ARCHIVE_MATERIALIZATION_STATE_MISSING")
    print("RAFCODEPHI_REAL_BOOTSTRAP_WORKFLOW=PASS canonical_overlay=true failure_evidence=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
