#!/usr/bin/env python3
"""Source contract tests for the staged producer/consumer artifact boundary."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
workflow = (ROOT / ".github/workflows/rafcodephi-auto-handoff.yml").read_text()
zipraf = (ROOT / "scripts/ci/rafcodephi_producer_zipraf.py").read_text()
consumer_contract = "rafcodephi-termux-packages-${{ github.sha }}"

assert "  contract:\n" in workflow
assert "  produce:\n" in workflow
assert "  handoff:\n" in workflow
assert "    needs: [contract, produce]" in workflow
assert "    if: github.event_name != 'pull_request'" in workflow
assert "python3 scripts/ci/rafcodephi_producer_zipraf.py --self-test" in workflow
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
assert workflow.index("  contract:") < workflow.index("  produce:") < workflow.index("  handoff:")
assert "if: github.event_name != 'pull_request'" in workflow[workflow.index("  handoff:"):]
assert "secrets.GITPAT || secrets.PATGITHUB || secrets.GIT" in workflow[workflow.index("  handoff:"):]
assert "producer_artifact.outputs.artifact-id" in workflow[workflow.index("  produce:"):workflow.index("  handoff:")]
assert '    "physical_android": "TOKEN_VAZIO"' in zipraf
assert '    "claim_allowed": False' in zipraf
assert ".lower().endswith(\".apk\")" in zipraf
assert "symlink_input_forbidden" in zipraf
assert "producer_sha256_mismatch" in zipraf
assert "zip_member_hash_mismatch" in zipraf
print("RAFCODEPHI_PIPELINE_CONTRACT=PASS jobs=3 handoff=preserved zipraf=non-apk")
