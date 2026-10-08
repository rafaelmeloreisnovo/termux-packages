#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/emit_rafcodephi_package_custody.py"
WORKFLOW = ROOT / ".github/workflows/rafcodephi-runtime-packages.yml"


def require(cond: bool, token: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED reason={token}")



def exercise_dynamic_static_resolution() -> None:
    """Tiny source+control fixture; no Termux rebuild and no network."""
    import runpy
    import tempfile
    from unittest.mock import patch

    module = runpy.run_path(str(SCRIPT))
    resolve = module["dynamic_static_producer"]
    with tempfile.TemporaryDirectory(prefix="rafcodephi-custody-static-") as temporary:
        root = Path(temporary)
        rule = root / "scripts/build/termux_create_debian_subpackages.sh"
        rule.parent.mkdir(parents=True)
        rule.write_text(
            (ROOT / "scripts/build/termux_create_debian_subpackages.sh").read_text(
                encoding="utf-8"
            ),
            encoding="utf-8",
        )
        recipe = root / "packages/attr/build.sh"
        recipe.parent.mkdir(parents=True)
        recipe.write_text("TERMUX_PKG_VERSION=2.5.2\n", encoding="utf-8")
        debs = {"attr": Path("attr"), "attr-static": Path("attr-static")}
        fields = {
            ("attr", "Version"): "2.5.2-1",
            ("attr-static", "Version"): "2.5.2-1",
            ("attr-static", "Depends"): "attr (= 2.5.2-1)",
            ("attr-static", "Description"): "Static libraries for attr",
        }

        def fake_field(path: Path, field: str) -> str:
            return fields[(path.name, field)]

        with patch.dict(resolve.__globals__, {"deb_field": fake_field}):
            require(resolve(root, "attr-static", debs) == "attr", "STATIC_PARENT_MISSING")
            require(resolve(root, "attr", debs) is None, "NON_STATIC_PROMOTED")
            require(
                resolve(root, "attr-static", {"attr-static": Path("attr-static")}) is None,
                "PARENT_ABSENT_PROMOTED",
            )
            fields[("attr-static", "Version")] = "2.5.2-2"
            require(resolve(root, "attr-static", debs) is None, "VERSION_DRIFT_ACCEPTED")
            fields[("attr-static", "Version")] = "2.5.2-1"
            fields[("attr-static", "Depends")] = "other (= 2.5.2-1)"
            require(resolve(root, "attr-static", debs) is None, "PARENT_DEPENDENCY_MISSING")
            fields[("attr-static", "Depends")] = "attr (= 2.5.2-1)"
            fields[("attr-static", "Description")] = "Unrelated archive"
            require(resolve(root, "attr-static", debs) is None, "DESCRIPTION_MISMATCH")
            fields[("attr-static", "Description")] = "Static libraries for attr"
            recipe.write_text("TERMUX_PKG_NO_STATICSPLIT=true\n", encoding="utf-8")
            require(resolve(root, "attr-static", debs) is None, "DISABLED_SPLIT_ACCEPTED")
            recipe.write_text("TERMUX_PKG_VERSION=2.5.2\n", encoding="utf-8")
            rule.write_text("unrecognized generator version\n", encoding="utf-8")
            try:
                resolve(root, "attr-static", debs)
            except SystemExit as exc:
                require("contract changed" in str(exc), "RULE_DRIFT_REASON_MISSING")
            else:
                raise SystemExit("STATIC_GENERATOR_DRIFT_ACCEPTED")
    print("RAFCODEPHI_STATIC_CUSTODY_FIXTURE=PASS positives=1 negatives=6")



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
    exercise_dynamic_static_resolution()
    print("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=PASS per_deb=true recipe_blob=true sha256=true fail_closed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
