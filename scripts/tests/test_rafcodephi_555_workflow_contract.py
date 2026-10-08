#!/usr/bin/env python3
"""Static 555 dispatch contract: enforce literal Bash and bounded evidence authority."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/555-artifact-manifold.yml"
SOURCE = WORKFLOW.read_text(encoding="utf-8")


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise SystemExit("RAFCODEPHI_555_CONTRACT=BLOCKED reason=" + reason)


def main() -> int:
    require(not re.search(r"\$\{\{\s*['\"]?\$[A-Za-z_]", SOURCE),
            "BASH_VAR_INTERPOLATED_AS_ACTIONS_EXPRESSION")
    for required in (
        'if [[ "$ORIGIN" == source-contract ]]; then',
        '[[ "$RUN_ID" =~ ^[1-9][0-9]*$ ]]',
        '[[ "$ARTIFACT_NAME" =~ ^[A-Za-z0-9._,-]{1,160}$ ]]',
        '[[ "$producer_sha" =~ ^[a-f0-9]{40}$ ]]',
        'if [[ "$ORIGIN" == artifact-run ]]; then root=inbound; fi',
        'rafcodephi-publish-dev-apt.yml',
        '.event == "pull_request"',
        '.head_repository.full_name == $repo',
        'if: always()',
        'actions: read',
        '--input-root "$root"',
        '--out _555',
    ):
        require(required in SOURCE, "MISSING:" + required[:75])
    require("build-rafcodephi-real-bootstrap.sh --architectures" not in SOURCE,
            "HEAVY_BUILD_IN_OBSERVATION_ONLY_555")
    require("RAFCODEPHI_555_SELF_TEST=PASS" not in SOURCE,
            "FAKE_TEST_RESULT_IN_WORKFLOW")
    print("RAFCODEPHI_555_WORKFLOW_CONTRACT=PASS no_rebuild=true shell_predicates=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
