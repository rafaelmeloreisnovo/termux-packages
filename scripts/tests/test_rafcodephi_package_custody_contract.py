#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/emit_rafcodephi_package_custody.py"
WORKFLOW = ROOT / ".github/workflows/rafcodephi-runtime-packages.yml"


def require(cond: bool, token: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED reason={token}")


def main() -> int:
    script = SCRIPT.read_text(encoding="utf-8")
    workflow = WORKFLOW.read_text(encoding="utf-8")

    for token in (
        "rafcodephi.package-custody-manifest/v1",
        "records_root_sha256",
        "deb_sha256",
        "recipe_git_blob",
        "recipe_sha256",
        "source_checksums_declared",
        "TOKEN_VAZIO_DECLARED_SOURCE_DIGEST",
        "physical_android",
        "claim_allowed",
    ):
        require(token in script, f"SCRIPT_TOKEN_MISSING:{token}")

    for token in (
        "Emit per-DEB custody manifest",
        "scripts/emit_rafcodephi_package_custody.py",
        "rafcodephi-package-custody.json",
        "rafcodephi-package-custody.json.sha256",
        "sha256sum -c rafcodephi-package-custody.json.sha256",
    ):
        require(token in workflow, f"WORKFLOW_TOKEN_MISSING:{token}")

    require("missing expected output .deb(s)" in script, "MISSING_OUTPUT_NOT_FAIL_CLOSED")
    require('"claim_allowed": False' in script, "CLAIM_BOUNDARY_MISSING")
    print("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=PASS per_deb=true recipe_blob=true sha256=true fail_closed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
