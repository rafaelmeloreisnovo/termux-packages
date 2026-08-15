#!/usr/bin/env python3
"""Static contract for retaining RAFCODEPHI ARM32 source-build evidence."""
from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_PUBLISH_DEV_APT_WORKFLOW=BLOCKED reason={token}")


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")
    require("rafcodephi-live-bootstrap.log" in text, "BUILD_LOG_CAPTURE_MISSING")
    require("rafcodephi-live-bootstrap-status.json" in text, "STATUS_RECEIPT_MISSING")
    require("build_status=\"${PIPESTATUS[0]}\"" in text, "DOCKER_EXIT_STATUS_LOST")
    require("DOCKER_RUNNER_MISSING" in text, "RUNNER_FAILURE_NOT_RECORDED")
    require("RAFCODEPHI_LIVE_APT_BUILD=BLOCKED" in text, "FAIL_CLOSED_STATUS_MISSING")
    require("- name: Upload source-build evidence\n        if: always()" in text, "FAILURE_ARTIFACT_NOT_ALWAYS_UPLOADED")
    require("output/*.deb" in text, "DEB_CLOSURE_EVIDENCE_MISSING")
    require("claim_allowed_release\": False" in text, "RELEASE_CLAIM_BOUNDARY_MISSING")
    require("Resolve development repository through isolated APT" in text, "ISOLATED_APT_GATE_MISSING")
    require("rafcodephi-isolated-apt.log" in text, "ISOLATED_APT_LOG_MISSING")
    require("rafcodephi-isolated-apt-status.json" in text, "ISOLATED_APT_RECEIPT_MISSING")
    require("apt-get \"${apt_options[@]}\" update" in text, "APT_UPDATE_NOT_EXECUTED")
    require("--download-only --no-install-recommends install \"$package_name\"" in text,
            "APT_DOWNLOAD_RESOLUTION_NOT_EXECUTED")
    require("RAFCODEPHI_ISOLATED_APT=BLOCKED" in text, "APT_FAILURE_NOT_FAIL_CLOSED")
    require("RAFCODEPHI_ISOLATED_APT=PASS" in text, "APT_PASS_NOT_EMITTED")
    print(
        "RAFCODEPHI_PUBLISH_DEV_APT_WORKFLOW=PASS "
        "source_build_log=true failure_artifact=true isolated_apt=true claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
