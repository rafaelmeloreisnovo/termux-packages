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
        "- name: Materialize flat signed APT repository",
    )
    require("output/*.deb" not in source_upload, "RAW_DEB_UPLOAD_PATH_NOT_PORTABLE")
    require("rafcodephi-arm32-source-built-debs.tar.sha256" in source_upload,
            "PORTABLE_DEB_DIGEST_NOT_UPLOADED")

    require("Resolve signed repository through isolated APT" in text, "ISOLATED_APT_GATE_MISSING")
    require("rafcodephi-isolated-apt.log" in text, "ISOLATED_APT_LOG_MISSING")
    require("rafcodephi-isolated-apt-status.json" in text, "ISOLATED_APT_RECEIPT_MISSING")
    require("apt-get \"${apt_options[@]}\" update" in text, "APT_UPDATE_NOT_EXECUTED")
    require("signed-by=%s/rafcodephi-archive-key.gpg" in text, "SIGNED_APT_CLIENT_MISSING")
    require("--download-only --no-install-recommends install \"$package_name\"" in text,
            "APT_DOWNLOAD_RESOLUTION_NOT_EXECUTED")
    require("RAFCODEPHI_ISOLATED_APT=BLOCKED" in text, "APT_FAILURE_NOT_FAIL_CLOSED")
    require("RAFCODEPHI_ISOLATED_APT=PASS" in text, "APT_PASS_NOT_EMITTED")

    require("Bundle portable signed APT repository evidence" in text, "PORTABLE_APT_BUNDLE_STEP_MISSING")
    require("rafcodephi-arm32-signed-apt-repository.tar" in text, "PORTABLE_APT_BUNDLE_MISSING")
    require("rafcodephi.portable-signed-apt-repository-evidence/v1" in text,
            "PORTABLE_APT_STATE_SCHEMA_MISSING")
    repo_upload = between(
        text,
        "- name: Upload bootstrap + signed repository evidence",
        "- name: Publish signed repository branch",
    )
    require("/tmp/rafcodephi-apt-publish/repo" not in repo_upload,
            "RAW_APT_REPOSITORY_UPLOAD_PATH_NOT_PORTABLE")
    require("rafcodephi-arm32-signed-apt-repository.tar.sha256" in repo_upload,
            "PORTABLE_APT_DIGEST_NOT_UPLOADED")


    require("type: boolean" in text and "publish:" in text, "EXPLICIT_PUBLISH_INPUT_MISSING")
    require("RAFCODEPHI_APT_SIGNING_KEY_B64" in text, "PRODUCTION_SIGNING_KEY_BINDING_MISSING")
    require("RAFCODEPHI_APT_SIGNING_FINGERPRINT" in text, "SIGNING_FINGERPRINT_GATE_MISSING")
    require("RAFCODEPHI_APT_PUBLISH_TOKEN" in text, "DEDICATED_PUBLISH_TOKEN_MISSING")
    require("Signed-By:" in text, "SIGNED_BY_BOOTSTRAP_GATE_MISSING")
    require("--clearsign --output InRelease Release" in text, "INRELEASE_SIGNATURE_MISSING")
    require("--detach-sign --output Release.gpg Release" in text, "RELEASE_GPG_SIGNATURE_MISSING")
    require("gpg --batch --verify Release.gpg Release" in text, "RELEASE_SIGNATURE_VERIFY_MISSING")
    require("signed-by=%s/rafcodephi-archive-key.gpg" in text, "ISOLATED_APT_SIGNED_BY_MISSING")
    require("github.event_name == 'workflow_dispatch' && inputs.publish == true" in text,
            "PUBLICATION_NOT_EXPLICITLY_GATED")
    require("git -C \"$publish_dir\" push origin" in text, "NON_FORCE_PUBLICATION_PUSH_MISSING")
    require("git push --force" not in text and "git -C \"$publish_dir\" push --force" not in text,
            "FORCE_PUSH_FORBIDDEN")
    require("deb [trusted=yes]" not in text, "TRUSTED_YES_FORBIDDEN")
    require("persist-credentials: true" not in text, "PERSISTED_CHECKOUT_CREDENTIAL_FORBIDDEN")

    print(
        "RAFCODEPHI_PUBLISH_DEV_APT_WORKFLOW=PASS "
        "source_build_log=true portable_deb_bundle=true isolated_apt=true "
        "portable_repo_bundle=true signed_repo=true explicit_publish_gate=true claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
