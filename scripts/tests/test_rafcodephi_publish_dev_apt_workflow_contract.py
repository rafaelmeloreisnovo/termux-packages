#!/usr/bin/env python3
"""Static contract for retaining portable RAFCODEPHI ARM32 build evidence."""
from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_PUBLISH_DEV_APT_WORKFLOW=BLOCKED reason={token}")


def between(text: str, start: str, end: str) -> str:
    require(start in text, f"SECTION_START_MISSING:{start}")
    require(end in text, f"SECTION_END_MISSING:{end}")
    return text.split(start, 1)[1].split(end, 1)[0]


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

    require("Bundle portable source-build evidence" in text, "PORTABLE_DEB_BUNDLE_STEP_MISSING")
    require("rafcodephi-arm32-source-built-debs.tar" in text, "PORTABLE_DEB_BUNDLE_MISSING")
    require("rafcodephi.portable-source-build-evidence/v1" in text, "PORTABLE_DEB_STATE_SCHEMA_MISSING")
    source_upload = between(
        text,
        "- name: Upload source-build evidence",
        "- name: Materialize flat APT repository",
    )
    require("output/*.deb" not in source_upload, "RAW_DEB_UPLOAD_PATH_NOT_PORTABLE")
    require("rafcodephi-arm32-source-built-debs.tar.sha256" in source_upload,
            "PORTABLE_DEB_DIGEST_NOT_UPLOADED")

    require("Resolve development repository through isolated APT" in text, "ISOLATED_APT_GATE_MISSING")
    require("rafcodephi-isolated-apt.log" in text, "ISOLATED_APT_LOG_MISSING")
    require("rafcodephi-isolated-apt-status.json" in text, "ISOLATED_APT_RECEIPT_MISSING")
    require("apt-get \"${apt_options[@]}\" update" in text, "APT_UPDATE_NOT_EXECUTED")
    require("--download-only --no-install-recommends install \"$package_name\"" in text,
            "APT_DOWNLOAD_RESOLUTION_NOT_EXECUTED")
    require("RAFCODEPHI_ISOLATED_APT=BLOCKED" in text, "APT_FAILURE_NOT_FAIL_CLOSED")
    require("RAFCODEPHI_ISOLATED_APT=PASS" in text, "APT_PASS_NOT_EMITTED")

    require("Bundle portable APT repository evidence" in text, "PORTABLE_APT_BUNDLE_STEP_MISSING")
    require("rafcodephi-arm32-dev-apt-repository.tar" in text, "PORTABLE_APT_BUNDLE_MISSING")
    require("rafcodephi.portable-apt-repository-evidence/v1" in text,
            "PORTABLE_APT_STATE_SCHEMA_MISSING")
    repo_upload = between(
        text,
        "- name: Upload bootstrap + repository evidence",
        "- name: Publish development repository branch",
    )
    require("/tmp/rafcodephi-apt-publish/repo" not in repo_upload,
            "RAW_APT_REPOSITORY_UPLOAD_PATH_NOT_PORTABLE")
    require("rafcodephi-arm32-dev-apt-repository.tar.sha256" in repo_upload,
            "PORTABLE_APT_DIGEST_NOT_UPLOADED")

    print(
        "RAFCODEPHI_PUBLISH_DEV_APT_WORKFLOW=PASS "
        "source_build_log=true portable_deb_bundle=true isolated_apt=true "
        "portable_repo_bundle=true claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
