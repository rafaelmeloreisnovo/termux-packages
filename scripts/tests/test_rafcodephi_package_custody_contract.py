#!/usr/bin/env python3
from pathlib import Path
import importlib.util
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/emit_rafcodephi_package_custody.py"
WORKFLOW = ROOT / ".github/workflows/rafcodephi-runtime-packages.yml"
SIGNED_APT_WORKFLOW = ROOT / ".github/workflows/rafcodephi-publish-dev-apt.yml"


def require(cond: bool, token: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED reason={token}")



def verify_generated_static_mapping() -> None:
    """Pure fixture: dynamically created subpackage binds to parent, not a guess."""
    spec = importlib.util.spec_from_file_location("custody", SCRIPT)
    require(spec is not None and spec.loader is not None, "IMPORT_SPEC_MISSING")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="custody-static-") as temp:
        root = Path(temp)
        recipe = root / "packages/attr/build.sh"
        recipe.parent.mkdir(parents=True)
        recipe.write_text("TERMUX_PKG_VERSION=2.5.2\\n", encoding="utf-8")
        generator = root / "scripts/build/termux_create_debian_subpackages.sh"
        generator.parent.mkdir(parents=True)
        generator.write_text(
            'TERMUX_PKG_NO_STATICSPLIT=false\\n'
            '_STATIC_SUBPACKAGE_FILE="$TERMUX_PKG_TMPDIR/${TERMUX_PKG_NAME}-static.subpackage.sh"\\n',
            encoding="utf-8",
        )
        require(module.resolve_auto_recipe(root, "attr-static")
                == ("attr", "GENERATED_STATIC_SUBPACKAGE"),
                "GENERATED_STATIC_PARENT_UNRESOLVED")
        # Negative cases: explicit opt-out and unrelated producer script.
        recipe.write_text("TERMUX_PKG_NO_STATICSPLIT=true\n", encoding="utf-8")
        try:
            module.resolve_auto_recipe(root, "attr-static")
        except ValueError:
            pass
        else:
            raise SystemExit("STATIC_SPLIT_OPT_OUT_NOT_BLOCKED")
        recipe.write_text("TERMUX_PKG_VERSION=2.5.2\n", encoding="utf-8")
        original_generator = generator.read_text(encoding="utf-8")
        generator.write_text("not a canonical static producer\n", encoding="utf-8")
        try:
            module.resolve_auto_recipe(root, "attr-static")
        except ValueError:
            pass
        else:
            raise SystemExit("GENERATOR_CONTRACT_MISSING_NOT_BLOCKED")
        generator.write_text(original_generator, encoding="utf-8")
        explicit = root / "packages/other/attr-static.subpackage.sh"
        explicit.parent.mkdir(parents=True)
        explicit.write_text("TERMUX_SUBPKG_DESCRIPTION=test\\n", encoding="utf-8")
        require(module.resolve_auto_recipe(root, "attr-static")
                == ("other", "DECLARED_SUBPACKAGE"),
                "EXPLICIT_SUBPACKAGE_NOT_PRIORITIZED")
        direct = root / "packages/attr-static/build.sh"
        direct.parent.mkdir(parents=True)
        direct.write_text("TERMUX_PKG_VERSION=1\\n", encoding="utf-8")
        try:
            module.resolve_auto_recipe(root, "attr-static")
        except ValueError:
            pass
        else:
            raise SystemExit("AMBIGUOUS_RECIPE_NOT_BLOCKED")
        try:
            module.resolve_auto_recipe(root, "unknown-static")
        except ValueError:
            pass
        else:
            raise SystemExit("UNVERIFIED_STATIC_PRODUCER_NOT_BLOCKED")


def main() -> int:
    script = SCRIPT.read_text(encoding="utf-8")
    workflow = WORKFLOW.read_text(encoding="utf-8")
    signed_apt = SIGNED_APT_WORKFLOW.read_text(encoding="utf-8")
    verify_generated_static_mapping()

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
    require("GENERATED_STATIC_SUBPACKAGE" in script, "STATIC_PRODUCER_MAP_MISSING")
    require("subpackage_generator_git_blob" in script, "STATIC_PRODUCER_BLOB_UNBOUND")
    require('(cd build/reports && sha256sum -c "rafcodephi-package-custody-$arch.json.sha256")' in signed_apt,
            "CUSTODY_SIDECAR_RELATIVE_PATH_REGRESSION")
    require('"source_commit": os.environ["GITHUB_SHA"]' in signed_apt,
            "CUSTODY_SOURCE_SHA_LITERAL_REGRESSION")
    workflow_555 = (ROOT / ".github/workflows/555-artifact-manifold.yml").read_text(encoding="utf-8")
    require('if ${{' not in workflow_555, "555_BASH_PREDICATE_INTERPOLATION_REGRESSION")
    for token in (
        'if [[ "$ORIGIN" == source-contract ]]; then',
        'if [[ "$ORIGIN" == artifact-run ]]; then root=inbound; fi',
        '[[ "$RUN_ID" =~ ^[1-9][0-9]*$ ]]',
        '[[ "$ARTIFACT_NAME" =~ ^[A-Za-z0-9._,-]{1,160}$ ]]',
        '[[ "$producer_sha" =~ ^[a-f0-9]{40}$ ]]',
    ):
        require(token in workflow_555, "555_PREDICATE_MISSING:" + token)
    require('"claim_allowed": False' in script, "CLAIM_BOUNDARY_MISSING")
    print("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=PASS per_deb=true recipe_blob=true sha256=true fail_closed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
