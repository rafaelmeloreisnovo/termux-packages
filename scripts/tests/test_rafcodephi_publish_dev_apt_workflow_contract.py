#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml"
LIVE = ROOT / "scripts/build-rafcodephi-live-bootstrap.sh"
GENERATOR = ROOT / "scripts/generate-bootstraps.sh"


def require(cond: bool, token: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_SIGNED_APT_WORKFLOW=BLOCKED reason={token}")


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")
    live = LIVE.read_text(encoding="utf-8")
    generator = GENERATOR.read_text(encoding="utf-8")

    required = (
        "Build ARM and ARM64 source closure + signed bootstrap pair",
        "--architectures arm,aarch64",
        "artifacts/rafcodephi-bootstrap/debs/arm",
        "artifacts/rafcodephi-bootstrap/debs/aarch64",
        "Emit complete per-DEB custody for ARM and ARM64",
        "--auto-map",
        "rafcodephi.package-custody-dualarch/v1",
        "dists/stable/main/binary-arm/Packages",
        "dists/stable/main/binary-aarch64/Packages",
        "dpkg-scanpackages -a arm",
        "dpkg-scanpackages -a aarch64",
        "pool/main/arm",
        "pool/main/aarch64",
        "--clearsign --output dists/stable/InRelease dists/stable/Release",
        "--detach-sign --output dists/stable/Release.gpg dists/stable/Release",
        "gpg --batch --verify dists/stable/Release.gpg dists/stable/Release",
        "Architectures=arm aarch64",
        "Components=main",
        "Suite=stable",
        "for arch in arm aarch64",
        "signed-by=%s/rafcodephi-archive-key.gpg",
        "RAFCODEPHI_ISOLATED_APT=PASS",
        "rafcodephi-dualarch-signed-apt-repository.tar",
        "RAFCODEPHI_APT_SIGNING_KEY_B64",
        "RAFCODEPHI_APT_SIGNING_FINGERPRINT",
        "RAFCODEPHI_APT_PUBLISH_TOKEN",
        "github.event_name == 'workflow_dispatch' && inputs.publish == true",
        "production publication must be dispatched from main",
        'git -C "$publish_dir" push origin',
    )
    for token in required:
        require(token in text, f"MISSING:{token}")

    live_required = (
        "Suites: stable",
        "Components: main",
        "Architectures: arm aarch64",
        "SIGNED_BY_ARCHIVE_KEY",
        "SIGNED_REPOSITORY_CONFIGURED",
    )
    for token in live_required:
        require(token in live, f"LIVE_BOOTSTRAP_MISSING:{token}")

    require("RAFCODEPHI_BOOTSTRAP_PACKAGE_EVIDENCE_DIR" in generator,
            "GENERATOR_DEB_EVIDENCE_HOOK_MISSING")
    require("sha256sum ./*.deb" in generator, "PER_ARCH_DEB_HASH_MISSING")

    forbidden = (
        "deb [trusted=yes]",
        "git push --force",
        'git -C "$publish_dir" push --force',
        "persist-credentials: true",
    )
    for token in forbidden:
        require(token not in text, f"FORBIDDEN:{token}")

    require("cancel-in-progress: ${{ github.event_name == 'pull_request' }}" in text,
            "STALE_PR_BUILD_CANCELLATION_MISSING")
    require("contents: read" in text, "READ_ONLY_DEFAULT_PERMISSION_MISSING")
    require("claim_allowed_release" in text, "CLAIM_BOUNDARY_MISSING")

    print(
        "RAFCODEPHI_SIGNED_APT_WORKFLOW=PASS "
        "dual_arch=true per_deb_custody=true signed_by=true "
        "release_signatures=true isolated_resolution=true "
        "manual_publish_only=true force_push=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
