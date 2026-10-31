#!/usr/bin/env python3
"""Stdlib-only fail-closed structural contract for the preview batch workflow.

Not a replacement for GitHub Actions execution or actual ARM/AArch64 DEB proof.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-batch-pipeline.yml"
OLD_PUBLISHER = ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml"


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_BATCH_CONTRACT=BLOCKED reason={reason}")


def check_workflow_text(text: str, publisher_present: bool = True) -> None:
    require(publisher_present, "ROLLBACK_PUBLISHER_MISSING")
    required = [
        "name: RAFCODEPHI Batch Pipeline (Candidate)",
        "default: false",
        "scripts/tests/test_rafcodephi_batch_pipeline_contract.py",
        "scripts/tests/test_rafcodephi_batch_pipeline_falsifiers.py",
        "permissions:\n  contents: read",
        "    needs: signed-apt-contract",
        "    needs: source-producer",
        "    needs: [signed-apt-contract, source-producer, signed-apt-proof]",
        "if: ${{ github.event_name == 'workflow_dispatch' && inputs.build == true }}",
        "        arch: [arm, aarch64]",
        "fail-fast: false",
        "RAFCODEPHI_APT_SIGNING_MODE=ephemeral-test",
        "rafcodephi-source-arm-${{ github.run_id }}-${{ github.run_attempt }}",
        "rafcodephi-source-aarch64-${{ github.run_id }}-${{ github.run_attempt }}",
        "rafcodephi-apt-preview-${{ github.run_id }}-${{ github.run_attempt }}",
        "rafcodephi-apt-proof-${{ matrix.arch }}-${{ github.run_id }}-${{ github.run_attempt }}",
        "sha256sum -c build/reports/rafcodephi-dualarch-signed-apt-repository.tar.sha256",
        "tf.extractall(dest, filter=\"data\")",
        "BATCH_PREVIEW=PASS_SCOPED; DEVICE=TOKEN_VAZIO; RELEASE=HOLD",
    ]
    missing = [part for part in required if part not in text]
    require(not missing, f"REQUIRED_MISSING:{missing}")
    forbidden = (
        "pull_request_target:", "contents: write", "id-token: write",
        "secrets.", "APT_PUBLISH_TOKEN", "git push", "Publish signed repository branch",
        "RELEASE=PASS", "CLAIM_ALLOWED=true",
    )
    present = [part for part in forbidden if part in text]
    require(not present, f"FORBIDDEN_PRESENT:{present}")
    # Only this exact negative check may mention the APT trust bypass token.
    # Checking the bare substring previously rejected the safe negated grep.
    guard = """! grep -Fq 'Trusted: yes' "build/reports/termux-$arch.sources" """
    guard = guard.rstrip()
    trust_lines = [line.strip() for line in text.splitlines() if "Trusted: yes" in line]
    require(trust_lines == [guard], f"APT_TRUST_GUARD_MISSING_OR_BYPASSED:{trust_lines!r}")
    require(text.count("uses: actions/upload-artifact@v7") >= 4, "ARTIFACT_UPLOADS_MISSING")
    require("      - name: Upload source/custody lot arm" in text, "ARM_UPLOAD_MISSING")
    require("      - name: Upload source/custody lot aarch64" in text, "AARCH64_UPLOAD_MISSING")
    require(text.count("if: ${{ always() }}") >= 5, "FAILURE_EVIDENCE_UPLOADS_MISSING")
    print("RAFCODEPHI_BATCH_CONTRACT=PASS_SOURCE_ONLY; BUILD=NOT_RUN; RELEASE=HOLD")


def check() -> None:
    check_workflow_text(WORKFLOW.read_text(encoding="utf-8"), OLD_PUBLISHER.is_file())


if __name__ == "__main__":
    check()
