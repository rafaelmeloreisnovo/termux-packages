#!/usr/bin/env python3
from pathlib import Path
import importlib.util
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/emit_rafcodephi_package_custody.py"
WORKFLOW = ROOT / ".github/workflows/rafcodephi-runtime-packages.yml"


def require(cond: bool, token: str) -> None:
    if not cond:
        raise SystemExit(f"RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED reason={token}")


def test_generated_static_recipe() -> None:
    """Source-backed synthetic falsifiers; no .deb or ARM/AArch64 rebuild."""
    spec = importlib.util.spec_from_file_location("custody_under_test", SCRIPT)
    require(spec is not None and spec.loader is not None, "CUSTODY_IMPORT_UNAVAILABLE")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="rafcodephi-custody-") as temp:
        root = Path(temp)
        packages = root / "packages"
        recipe = packages / "attr" / "build.sh"
        recipe.parent.mkdir(parents=True)
        recipe.write_text('TERMUX_PKG_VERSION="2.5.2"\n', encoding="utf-8")
        generator = root / "scripts/build/termux_create_debian_subpackages.sh"
        generator.parent.mkdir(parents=True)
        generator.write_text(
            'TERMUX_PKG_NO_STATICSPLIT TERMUX_PKG_TMPDIR '
            '${TERMUX_PKG_NAME}-static.subpackage.sh\n', encoding="utf-8"
        )
        require(
            module.resolve_auto_recipe(packages, "attr-static", generator)
            == ("attr", "GENERATED_STATIC_SUBPACKAGE"),
            "GENERATED_ATTR_STATIC_NOT_RESOLVED",
        )
        require(
            module.resolve_auto_recipe(packages, "attr", generator)
            == ("attr", "DIRECT_RECIPE"),
            "DIRECT_RECIPE_REGRESSED",
        )
        for pkg, bad_generator in (
            ("ghost-static", generator),
            ("attr-static", root / "absent_generator.sh"),
        ):
            try:
                module.resolve_auto_recipe(packages, pkg, bad_generator)
            except ValueError:
                pass
            else:
                raise SystemExit("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED "
                                 f"reason=INVENTED_RECIPE_ACCEPTED:{pkg}")
        generator.write_text("no generated static split contract\n", encoding="utf-8")
        try:
            module.resolve_auto_recipe(packages, "attr-static", generator)
        except ValueError:
            pass
        else:
            raise SystemExit("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=BLOCKED "
                             "reason=UNSUPPORTED_STATIC_GENERATOR_ACCEPTED")
    print("RAFCODEPHI_CUSTODY_DYNAMIC_STATIC_FIXTURE=PASS no_rebuild=true")


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

    test_generated_static_recipe()
    require("GENERATED_STATIC_SUBPACKAGE" in script, "DYNAMIC_MAPPING_CONTRACT_MISSING")
    require("missing expected output .deb(s)" in script, "MISSING_OUTPUT_NOT_FAIL_CLOSED")
    require('"claim_allowed": False' in script, "CLAIM_BOUNDARY_MISSING")
    print("RAFCODEPHI_PACKAGE_CUSTODY_CONTRACT=PASS per_deb=true recipe_blob=true sha256=true fail_closed=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
