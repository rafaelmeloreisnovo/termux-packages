#!/usr/bin/env python3
"""Source contract tests for the staged producer/consumer artifact boundary."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
workflow = (ROOT / ".github/workflows/rafcodephi-auto-handoff.yml").read_text()
zipraf = (ROOT / "scripts/ci/rafcodephi_producer_zipraf.py").read_text()
batch_verifier = (ROOT / "scripts/ci/rafcodephi_parallel_batches.py").read_text()
consumer_contract = "rafcodephi-termux-packages-${{ github.sha }}"

assert "  contract:\n" in workflow
assert "  produce:\n" in workflow
assert "  verify-batch:\n" in workflow
assert "  join-batches:\n" in workflow
assert "  handoff:\n" in workflow
assert "    needs: [contract, produce]" in workflow
assert "    needs: [contract, produce, verify-batch]" in workflow
assert "    needs: [contract, produce, join-batches]" in workflow
assert "    if: github.event_name != 'pull_request'" in workflow
assert "python3 scripts/ci/rafcodephi_producer_zipraf.py --self-test" in workflow
assert "python3 scripts/ci/rafcodephi_parallel_batches.py --self-test" in workflow
assert "max-parallel: 2" in workflow
assert "fail-fast: false" in workflow
assert "matrix:\n        arch: [arm, aarch64]" in workflow
assert "actions/download-artifact@v8" in workflow
assert "merge-multiple: true" in workflow
assert "rafcodephi-batch-join-" in workflow
assert "Bundle immutable non-APK producer evidence as ZIPRAF" in workflow
assert "rafcodephi-producer-evidence.zip" in workflow
assert "rafcodephi-producer-zipraf-${{ github.sha }}" in workflow
assert consumer_contract in workflow
assert "    name: Build + attest ARM/ARM64 producer" in workflow
for name in [
    "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt",
    "rafcodephi-bootstrap-arm.zip",
    "rafcodephi-bootstrap-aarch64.zip",
    "rafcodephi-auto-handoff.json",
    "producer_artifact.outputs.artifact-id",
    "producer_artifact.outputs.artifact-digest",
    "needs.produce.outputs.handoff_receipt_sha256",
    "rafcodephi_packages_ready",
    "termux_packages_sha",
    "producer_run_id",
    "artifact_name",
    "receipt_sha256",
    "artifact_id",
    "artifact_digest",
]:
    assert name in workflow, name
assert workflow.index("  contract:") < workflow.index("  produce:") < workflow.index("  verify-batch:") < workflow.index("  join-batches:") < workflow.index("  handoff:")
assert "needs: [contract, produce, join-batches]" in workflow[workflow.index("  handoff:"):]
assert "needs: [contract, produce, verify-batch]" in workflow[workflow.index("  join-batches:"):workflow.index("  handoff:")]
assert "needs: [contract, produce]" in workflow[workflow.index("  verify-batch:"):workflow.index("  join-batches:")]
assert "if: github.event_name != 'pull_request'" in workflow[workflow.index("  verify-batch:"):workflow.index("  join-batches:")]
assert "if: github.event_name != 'pull_request'" in workflow[workflow.index("  join-batches:"):workflow.index("  handoff:")]
assert "if: github.event_name != 'pull_request'" in workflow[workflow.index("  handoff:"):]
assert "secrets.GITPAT || secrets.PATGITHUB || secrets.GIT" in workflow[workflow.index("  handoff:"):]
assert "producer_artifact.outputs.artifact-id" in workflow[workflow.index("  produce:"):workflow.index("  handoff:")]
assert '    "physical_android": "TOKEN_VAZIO"' in zipraf
assert '    "claim_allowed": False' in zipraf
assert ".lower().endswith(\".apk\")" in zipraf
assert "symlink_input_forbidden" in zipraf
assert "producer_sha256_mismatch" in zipraf
assert "zip_member_hash_mismatch" in zipraf
assert 'SCHEMA = "rafcodephi.producer-arch-batch/v1"' in batch_verifier
assert 'JOIN_SCHEMA = "rafcodephi.producer-arch-batch-join/v1"' in batch_verifier
assert 'REQUIRED_ZIP_ENTRIES = ("BOOTSTRAP_PROFILE.json", "SYMLINKS.txt")' in batch_verifier
assert "bootstrap_hash_mismatch" in batch_verifier
assert "cross_arch_manifest_mismatch" in batch_verifier
assert "batch_receipt_mismatch" in batch_verifier
assert "receipt_overwrite_forbidden" in batch_verifier

# Change-sensitive producer: source writes only; no unrelated main pushes.
# The PR contract job stays cheap, but a main producer must never be
# cancelled mid-build (its ZIPRAF, digest and dispatch have one exact SHA).
trigger = workflow.split("\npermissions:\n", 1)[0]
push = trigger.split("  push:", 1)[1].split("  pull_request:", 1)[0]
assert "branches: [main]" in push
for source_path in (
    "scripts/**", "packages/**", "core/**", "configs/**",
    ".github/workflows/rafcodephi-auto-handoff.yml",
):
    assert source_path in push, source_path
assert "docs/**" not in push
assert "  workflow_dispatch:" in trigger
assert "group: rafcodephi-producer-${{ github.event_name }}-${{ github.ref }}" in workflow
assert "cancel-in-progress: ${{ github.event_name == 'pull_request' }}" in workflow

print("RAFCODEPHI_PIPELINE_CONTRACT=PASS jobs=5 matrix=arm+aarch64 fan-in=fail-closed handoff=preserved")
