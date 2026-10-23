#!/usr/bin/env python3
"""Falsify missing/invalid GitHub Actions triggers without executing any workflows."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/tests/test_rafcodephi_workflow_syntax.py"
spec = importlib.util.spec_from_file_location("rafcodephi_workflow_syntax", SCRIPT)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

JOB = """jobs:
  verify:
    runs-on: ubuntu-24.04
    steps:
      - name: Bash parse
        run: |
          set -euo pipefail
          printf 'contract-ok\\n'
"""

CASES = [
    ("map_trigger", "on:\\n  pull_request:\\n", True, None),
    ("scalar_trigger", "on: workflow_dispatch\\n", True, None),
    ("list_trigger", "on: [push, pull_request]\\n", True, None),
    ("missing_trigger", "", False, "github_triggers_missing"),
    ("empty_trigger_map", "on: {}\\n", False, "github_triggers_missing"),
    ("empty_trigger_list", "on: []\\n", False, "github_triggers_missing"),
    ("unknown_trigger", "on: puch\\n", False, "github_trigger_unknown"),
    ("boolean_off_trigger", "on: off\\n", False, "github_trigger_unknown"),
    ("null_trigger", "on:\\n", False, "github_triggers_missing"),
]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix=".rafcodephi-triggers-", dir=ROOT) as temp:
        path = Path(temp) / "fixture.yml"
        for case, trigger, accepted, reason in CASES:
            path.write_text("name: Trigger fixture\\n" + trigger + JOB, encoding="utf-8")
            try:
                result = module.validate_workflow(path)
            except SystemExit as error:
                if accepted or reason not in str(error):
                    raise AssertionError(f"{case}: unexpected BLOCKED={error}") from error
            else:
                if not accepted or result != 1:
                    raise AssertionError(f"{case}: unexpected ACCEPTED={result}")
    print(
        "RAFCODEPHI_TRIGGER_CONTRACT=PASS "
        "positive=3 negative=6 missing=blocked yaml11_on_safe=true "
        "android_runtime=NOT_RUN claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
