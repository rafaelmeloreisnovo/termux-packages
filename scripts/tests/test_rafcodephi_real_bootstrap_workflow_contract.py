#!/usr/bin/env python3
"""Static contract for the source-build bootstrap GitHub workflow."""
from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/rafcodephi-real-bootstrap.yml"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_REAL_BOOTSTRAP_WORKFLOW=BLOCKED reason={token}")


def exercise_ninja_sealer(builder: str) -> None:
    """Run the actual source sealer, including missing-tool falsifiers."""
    import ast
    import json
    import subprocess
    import sys
    import tempfile
    import zipfile

    marker = '''python3 - "$zip_path" "$PACKAGE_NAME" "$TARGET_PREFIX" "$arch" "$API_RECEIVER_COMPONENT" <<'PY'\n'''
    require(builder.count(marker) == 1, "SOURCE_SEALER_UNRESOLVED")
    script = builder.split(marker, 1)[1].split("\nPY\n", 1)[0]
    required = next(ast.literal_eval(n.value) for n in ast.parse(script).body
                    if isinstance(n, ast.Assign) and any(
                        isinstance(t, ast.Name) and t.id == "required" for t in n.targets))
    require("bin/ninja" in required, "NINJA_NOT_A_REQUIRED_INSTALLED_ENTRY")
    require("--add busybox,proot,ca-certificates,termux-api,ninja" in builder,
            "NINJA_SOURCE_PACKAGE_NOT_REQUESTED")
    with tempfile.TemporaryDirectory(prefix="raf-ninja-sealer-") as temp:
        for arch, missing in (("arm", False), ("aarch64", False), ("arm", True)):
            archive = Path(temp) / f"{arch}-{missing}.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                for name in required:
                    if name in ("BOOTSTRAP_INFO", "bin/sh", "etc/apt/apt.conf.d/00rafcodephi-repository-block"):
                        continue
                    if name == "bin/ninja" and missing:
                        continue
                    zf.writestr(name, "dash←bin/sh\n" if name == "SYMLINKS.txt" else "synthetic-not-runtime")
            proc = subprocess.run([sys.executable, "-", str(archive), "com.termux.rafacodephi",
                "/data/data/com.termux.rafacodephi/files/usr", arch,
                "com.termux.rafacodephi.api/com.termux.api.TermuxApiReceiver"],
                input=script, text=True, capture_output=True)
            if missing:
                require(proc.returncode != 0 and "bin/ninja" in proc.stderr,
                        "MISSING_NINJA_SEALED_AS_SUCCESS")
            else:
                require(proc.returncode == 0, f"NINJA_SEALER_FAILED:{proc.stderr[-200:]}")
                with zipfile.ZipFile(archive) as zf:
                    profile = json.loads(zf.read("BOOTSTRAP_PROFILE.json"))
                    require(profile["build_tools_contract"] == "rafcodephi.apt-bootstrap-ninja/v1",
                            "NINJA_CONTRACT_NOT_SEALED")
                    require(b"RAFCODEPHI_BUILD_TOOLS_CONTRACT=rafcodephi.apt-bootstrap-ninja/v1\n"
                            in zf.read("BOOTSTRAP_INFO"), "NINJA_INFO_NOT_SEALED")
    print("RAFCODEPHI_NINJA_SEALER=PASS cases=3 scope=HOST_SYNTHETIC")


def sealed_transport_complete(text: str) -> bool:
    """The handoff must actually upload the exact TAR and its independent digest."""
    marker = "      - name: Upload real bootstrap artifacts\n        if: success()"
    if text.count(marker) != 1:
        return False
    block = text.split(marker, 1)[1].split("      - name: Upload source-build diagnostics", 1)[0]
    if block.count("          path: |\n") != 1:
        return False
    paths = block.split("          path: |\n", 1)[1].split("\n          if-no-files-found:", 1)[0]
    named = {line.strip() for line in paths.splitlines() if line.strip() and not line.lstrip().startswith("#")}
    return {"artifacts/rafcodephi-bootstrap-evidence.tar.gz",
            "artifacts/rafcodephi-bootstrap-evidence.tar.gz.sha256"}.issubset(named)


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")
    require("scripts/apply-rafcodephi-build-properties.py" in text, "CANONICAL_PROPERTIES_OVERLAY_MISSING")
    require("scripts/validate-rafcodephi-build-properties.sh" in text, "PROPERTIES_VALIDATION_MISSING")
    require("path.write_text" not in text, "INLINE_PROPERTIES_MUTATION_PRESENT")
    require("scripts/tests/test_rafcodephi_prefix_gate.py" in text,
            "FREESTANDING_PREFIX_REGRESSION_TEST_NOT_ROUTED")
    require("scripts/tests/test_rafcodephi_proot_prefix_patch.py" in text,
            "PROOT_PREFIX_SOURCE_PATCH_TEST_NOT_ROUTED")
    require("scripts/ci/rafcodephi_bootstrap_prefix_gate.py" in text,
            "COMPLETE_BOOTSTRAP_PREFIX_GATE_NOT_IN_SCOPE")
    require("scripts/ci/rafcodephi_deb_set_gate.py" in text,
            "DEB_INVENTORY_GATE_NOT_IN_SCOPE")
    require("scripts/tests/test_rafcodephi_deb_set_gate.py" in text,
            "DEB_GATE_FALSIFIERS_NOT_RUN")
    require('prefix_gate_state_${arch}=PASS_SCOPED' in text,
            "CROSS_ARCH_PREFIX_RECEIPT_NOT_VERIFIED")
    require("rafcodephi-real-bootstrap.log" in text, "BUILD_LOG_CAPTURE_MISSING")
    require("rafcodephi-real-bootstrap-status.json" in text, "BUILD_STATUS_RECEIPT_MISSING")
    require("build_status=\"${PIPESTATUS[0]}\"" in text, "DOCKER_EXIT_STATUS_LOST")
    require("RAFCODEPHI_REAL_BOOTSTRAP_BUILD=BLOCKED" in text, "FAIL_CLOSED_STATUS_MISSING")
    require("scripts/ci/rafcodephi_prepare_ndk29.sh" in text,
            "NDK29_SHA_BOUND_PROVISIONER_MISSING")
    require("rafcodephi-ndk29-prepare.log" in text,
            "NDK29_PROVISIONING_RECEIPT_MISSING")
    require(text.count("NDK=/home/builder/.termux-build/android-ndk-r29") >= 2,
            "NDK29_NOT_BOUND_TO_PREFLIGHT_AND_BUILD")
    require("package-builder@sha256:09cdbb34" in text,
            "BUILDER_IMAGE_DIGEST_NOT_PINNED")
    require("- name: Upload real bootstrap artifacts\n        if: always()" in text,
            "FAILURE_ARTIFACT_NOT_ALWAYS_UPLOADED")
    require("- name: Seal DEB evidence for portable artifact upload\n        if: always()" in text,
            "DEB_TRANSPORT_NOT_FAILSAFE")
    require('tar -C "$root" -cf "$root/rafcodephi-debs.tar" debs' in text,
            "EPOCH_DEB_NAMES_NOT_ARCHIVED")
    require("sha256sum -c rafcodephi-debs.tar.sha256" in text,
            "DEB_TRANSPORT_HASH_NOT_VERIFIED")
    require("!artifacts/rafcodephi-bootstrap/debs/**" in text,
            "RAW_EPOCH_DEBS_STILL_EXPOSED_TO_UPLOAD")
    require(sealed_transport_complete(text), "SEALED_ABI_EVIDENCE_NOT_UPLOADED_WITH_DIGEST")
    for missing in ("artifacts/rafcodephi-bootstrap-evidence.tar.gz",
                    "artifacts/rafcodephi-bootstrap-evidence.tar.gz.sha256"):
        mutated = text.replace("            " + missing + "\n", "", 1)
        require(mutated != text and not sealed_transport_complete(mutated),
                "SEALED_TRANSPORT_MISSING_FILE_FALSIFIER_FAILED:" + missing)
    # Prevent upload YAML from being spliced inside the shell grep.
    require("""grep -Eq '^debs/(arm|aarch64)/[^/]+[.]deb$' "$root/rafcodephi-debs.tar.members" """ .strip() in text,
            "DEB_MEMBER_GREP_BROKEN_OR_UNBOUNDED")
    builder = (ROOT / "scripts/build-rafcodephi-real-bootstrap.sh").read_text(encoding="utf-8")
    exercise_ninja_sealer(builder)
    generator = (ROOT / "scripts/generate-bootstraps.sh").read_text(encoding="utf-8")
    docker_wrapper = (ROOT / "scripts/run-rafcodephi-bootstrap-docker.sh").read_text(encoding="utf-8")
    require('"runtime_materialized": False' in builder,
            "ARCHIVE_MATERIALIZATION_STATE_MISSING")
    require('TERMUX_BOOTSTRAP_OUTPUT_DIR' in generator,
            "WRITABLE_BOOTSTRAP_OUTPUT_HOOK_MISSING")
    require('BOOTSTRAP_OUTPUT_DIR="${TERMUX_BOOTSTRAP_OUTPUT_DIR:-.}"' in generator,
            "UPSTREAM_DEFAULT_OUTPUT_COMPATIBILITY_MISSING")
    require('"$BOOTSTRAP_OUTPUT_DIR/"' in generator,
            "BOOTSTRAP_ARCHIVE_NOT_ROUTED_TO_OUTPUT_DIR")
    require('export TERMUX_BOOTSTRAP_OUTPUT_DIR="$GENERATED_BOOTSTRAP_DIR"' in builder,
            "RAFCODEPHI_WRITABLE_GENERATED_OUTPUT_NOT_BOUND")
    require('zip_path="$GENERATED_BOOTSTRAP_DIR/bootstrap-${arch}.zip"' in builder,
            "RAFCODEPHI_GENERATED_ZIP_ROUTE_MISSING")
    require("run-rafcodephi-bootstrap-docker.sh" in text,
            "CANONICAL_DOCKER_HANDOFF_NOT_USED")
    require('RAFCODEPHI_BOOTSTRAP_OUT_DIR="$CONTAINER_OUT"' in docker_wrapper,
            "CONTAINER_LOCAL_OUTPUT_NOT_FORCED")
    require('docker_cmd[@]' in docker_wrapper and ' cp "$CONTAINER:$CONTAINER_OUT/." "$HOST_OUT/"' in docker_wrapper,
            "DOCKER_COPY_BACK_MISSING")
    require('RAFCODEPHI_BOOTSTRAP_DOCKER=PASS' in docker_wrapper,
            "DOCKER_HANDOFF_RECEIPT_MISSING")
    require('while link.startswith("./"):' in builder,
            "SYMLINK_DESTINATION_NORMALIZATION_MISSING")
    require(builder.count('while link.startswith("./"):') >= 2,
            "SYMLINK_DESTINATION_NORMALIZATION_NOT_APPLIED_TO_BOTH_GATES")
    require("termux-api-broadcast←(\\./)?libexec/termux-api" in builder,
            "TERMUX_API_SYMLINK_OPTIONAL_DOT_SLASH_GATE_MISSING")
    repair = (ROOT / "scripts/repair_rafcodephi_bootstrap_symlinks.py").read_text(encoding="utf-8")
    require("rafcodephi_bootstrap_prefix_gate.py" in builder,
            "FULL_PAYLOAD_BINARY_PREFIX_GATE_MISSING")
    require('prefix_gate_receipt_sha256_%s' in builder,
            "HASH_BOUND_PREFIX_AUDIT_RECEIPT_MISSING")
    require('prefix_gate_state_%s=PASS_SCOPED' in builder,
            "SOURCE_BUILD_SUCCESS_SCOPE_INCORRECT")
    require("rafcodephi_deb_set_gate.py" in builder,
            "SOURCE_DEB_SET_GATE_NOT_CALLED")
    require('deb_gate_receipt_sha256_%s' in builder,
            "HASH_BOUND_DEB_GATE_RECEIPT_MISSING")
    require('deb_gate_state_%s=PASS_SCOPED' in builder,
            "SOURCE_DEB_CLAIM_SCOPE_INVALID")
    require("repair_rafcodephi_bootstrap_symlinks.py" in builder,
            "DEB_EVIDENCE_SYMLINK_REPAIR_NOT_CALLED")
    require('--deb-dir "$RAFCODEPHI_BOOTSTRAP_PACKAGE_EVIDENCE_DIR/$arch"' in builder,
            "DEB_EVIDENCE_DIR_NOT_BOUND_TO_ARCH")
    require('rafcodephi-bootstrap-${arch}-symlink-repair.json' in builder,
            "SYMLINK_REPAIR_RECEIPT_NOT_BOUND")
    for token in (
        "rafcodephi.bootstrap-symlink-evidence-repair/v1",
        '"bin/sh": "dash"',
        '"libexec/termux-api": "termux-api-broadcast"',
        '"dpkg-deb", "--fsys-tarfile"',
        '"claim_allowed": False',
        '"device_runtime_proof": "TOKEN_VAZIO"',
    ):
        require(token in repair, f"SYMLINK_REPAIR_CONTRACT_MISSING:{token}")
    print("RAFCODEPHI_REAL_BOOTSTRAP_WORKFLOW=PASS canonical_overlay=true failure_evidence=true writable_archive_output=true canonical_docker_handoff=true deb_evidence_symlink_repair=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
