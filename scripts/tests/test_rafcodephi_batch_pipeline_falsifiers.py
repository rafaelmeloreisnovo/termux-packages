#!/usr/bin/env python3
"""Negative source-only tests for the candidate batch YAML contract.

Does not run APT, Docker, signing, external services or device operations.
"""
from test_rafcodephi_batch_pipeline_contract import (
    WORKFLOW,
    OLD_PUBLISHER,
    check_workflow_text,
)


def must_block(name: str, changed: str) -> None:
    try:
        check_workflow_text(changed, publisher_present=True)
    except SystemExit as error:
        if "RAFCODEPHI_BATCH_CONTRACT=BLOCKED" not in str(error):
            raise
        return
    raise SystemExit(f"RAFCODEPHI_BATCH_FALSIFIER=FAIL accepted={name}")


def main() -> None:
    original = WORKFLOW.read_text(encoding="utf-8")
    check_workflow_text(original, publisher_present=OLD_PUBLISHER.is_file())
    guard = """! grep -Fq 'Trusted: yes' "build/reports/termux-$arch.sources" """
    guard = guard.rstrip()
    if original.count(guard) != 1:
        raise SystemExit("RAFCODEPHI_BATCH_FALSIFIER=BLOCKED guard_count")

    must_block("trust_guard_removed", original.replace(guard, "", 1))
    must_block("trust_guard_negation_removed", original.replace(guard, guard[2:], 1))
    must_block("positive_trust_directive", original + "\nTrusted: yes\n")
    must_block("write_permission", original.replace("contents: read", "contents: write", 1))
    must_block("auto_expensive_dispatch", original.replace("default: false", "default: true", 1))
    must_block(
        "missing_test_trigger",
        original.replace("      - 'scripts/tests/test_rafcodephi_batch_pipeline_falsifiers.py'\n", "", 1),
    )
    must_block(
        "missing_failure_artifact",
        original.replace("if: ${{ always() }}", "if: ${{ success() }}", 1),
    )
    try:
        check_workflow_text(original, publisher_present=False)
    except SystemExit:
        pass
    else:
        raise SystemExit("RAFCODEPHI_BATCH_FALSIFIER=FAIL missing_rollback_publisher")
    print("RAFCODEPHI_BATCH_FALSIFIERS=PASS_SCOPED cases=8 RELEASE=HOLD")


if __name__ == "__main__":
    main()
