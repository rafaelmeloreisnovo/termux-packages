#!/usr/bin/env python3
"""Stdlib-only fail-closed structural contract for the preview batch workflow.

Not a replacement for GitHub Actions execution or actual ARM/AArch64 DEB proof.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-batch-pipeline.yml"
OLD_PUBLISHER = ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml"


def check() -> None:
    text = WORKFLOW.read_text(encoding="utf-8")
    assert OLD_PUBLISHER.is_file(), "rollback publisher missing"
    required = [
        "name: RAFCODEPHI Batch Pipeline (Candidate)",
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
    assert not missing, f"required: {missing}"
    forbidden = (
        "pull_request_target:", "contents: write", "id-token: write",
        "secrets.", "APT_PUBLISH_TOKEN", "git push", "Publish signed repository branch",
        "Trusted: yes", "RELEASE=PASS", "CLAIM_ALLOWED=true",
    )
    present = [part for part in forbidden if part in text]
    assert not present, f"forbidden: {present}"
    assert text.count("uses: actions/upload-artifact@v7") >= 4
    assert "      - name: Upload source/custody lot arm" in text
    assert "      - name: Upload source/custody lot aarch64" in text
    assert text.count("if: ${{ always() }}") >= 5
    print("RAFCODEPHI_BATCH_CONTRACT=PASS_SOURCE_ONLY; BUILD=NOT_RUN; RELEASE=HOLD")


if __name__ == "__main__":
    check()
