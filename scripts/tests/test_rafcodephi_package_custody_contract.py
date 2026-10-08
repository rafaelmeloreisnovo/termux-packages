#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/emit_rafcodephi_package_custody.py"
WORKFLOW = ROOT / ".github/workflows/rafcodephi-runtime-packages.yml"


def require(cond: bool, token: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED reason={token}")


def test_generated_static_recipe() -> None:
    """No-build falsifiers: generated split, explicit opt-out and missing authority."""
    import importlib.util
    import tempfile

    spec = importlib.util.spec_from_file_location("custody_module", SCRIPT)
    require(spec is not None and spec.loader is not None, "CUSTODY_IMPORT_UNAVAILABLE")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="rafcodephi-custody-map-") as tmp:
        root = Path(tmp)
        recipe = root / "packages/attr/build.sh"
        generator = root / "scripts/build/termux_create_debian_subpackages.sh"
        recipe.parent.mkdir(parents=True)
        generator.parent.mkdir(parents=True)
        recipe.write_text('TERMUX_PKG_VERSION=2.5.2\n', encoding="utf-8")
        generator.write_text(
            'TERMUX_PKG_NO_STATICSPLIT ${TERMUX_PKG_NAME}-static.subpackage.sh TERMUX_PKG_TMPDIR\n',
            encoding="utf-8",
        )
        require(module.resolve_auto_static_recipe(root, "attr-static") == "attr",
                "AUTOSTATIC_PRODUCER_NOT_RESOLVED")
        require(module.resolve_auto_static_recipe(root, "attr") is None,
                "NONSTATIC_PRODUCER_FALSE_MATCH")
        require(module.resolve_auto_static_recipe(root, "other-static") is None,
                "MISSING_PRODUCER_FALSE_MATCH")
        recipe.write_text("TERMUX_PKG_NO_STATICSPLIT=true\n", encoding="utf-8")
        require(module.resolve_auto_static_recipe(root, "attr-static") is None,
                "STATIC_SPLIT_OPT_OUT_IGNORED")
        recipe.write_text("TERMUX_PKG_NO_STATICSPLIT=false\n", encoding="utf-8")
        generator.write_text("unrelated build script\n", encoding="utf-8")
        require(module.resolve_auto_static_recipe(root, "attr-static") is None,
                "STATIC_GENERATOR_UNBOUND")
    print("RAFCODEPHI_CUSTODY_AUTOSTATIC_FIXTURE=PASS no_build=true")


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
        "resolve_auto_static_recipe",
        "producer_generator_git_blob",
        "GENERATED_STATIC_SPLIT",
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
    workflow_555 = (ROOT / ".github/workflows/555-artifact-manifold.yml").read_text(encoding="utf-8")
    require('if ${{' not in workflow_555, "555_RUNTIME_PREDICATE_BASH_INTERPOLATION")
    require('if [[ "$ORIGIN" == source-contract ]]; then' in workflow_555,
            "555_SOURCE_PREDICATE_MISSING")
    require('if [[ "$ORIGIN" == artifact-run ]]; then root=inbound; fi' in workflow_555,
            "555_ARTIFACT_PREDICATE_MISSING")
    require('[[ "$RUN_ID" =~ ^[1-9][0-9]*$ ]]' in workflow_555,
            "555_RUN_ID_VALIDATION_MISSING")
    test_generated_static_recipe()
    print("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=PASS per_deb=true recipe_blob=true sha256=true fail_closed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
