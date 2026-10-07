#!/usr/bin/env python3
"""Static fail-closed contract for the embedded RAFCODEPHI Termux:API CLI."""
from __future__ import annotations

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PATCH = (ROOT / "packages/termux-api/termux-api.c.patch").read_text(encoding="utf-8")
BUILDER = (ROOT / "scripts/build-rafcodephi-real-bootstrap.sh").read_text(encoding="utf-8")
PATCHER = (ROOT / "scripts/build/termux_step_patch_package.sh").read_text(encoding="utf-8")
TARGET = "com.termux.rafacodephi.api/com.termux.api.TermuxApiReceiver"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_TERMUX_API_CONTRACT=BLOCKED reason={token}")


def main() -> int:
    require(f'+    child_argv[5] = "{TARGET}";' in PATCH, "API_RECEIVER_TARGET_MISSING")
    require(
        '+# define PREFIX "@TERMUX_PREFIX@"' in PATCH,
        "TERMUX_API_PREFIX_TEMPLATE_MISSING",
    )
    require(
        r's%\@TERMUX_PREFIX\@%${TERMUX_PREFIX}%g' in PATCHER,
        "TERMUX_PREFIX_PATCH_SUBSTITUTION_MISSING",
    )
    require('API_RECEIVER_COMPONENT="${PACKAGE_NAME}.api/com.termux.api.TermuxApiReceiver"' in BUILDER,
            "API_RECEIVER_CLASS_IDENTITY_MISSING")
    require(
        "RAFCODEPHI_SHARED_UID_FILESYSTEM_SOCKETS" in PATCH,
        "CROSS_UID_ABSTRACT_SOCKET_ROUTE_MISSING",
    )
    require('+    child_argv[1] = "startservice";' not in PATCH, "STARTSERVICE_STUB_ROUTE_PRESENT")
    require("com.termux/com.termux.app.TermuxService" not in PATCH, "MAIN_SERVICE_STUB_COMPONENT_PRESENT")
    require("com.termux.service_api" not in PATCH, "UNHANDLED_SERVICE_ACTION_PRESENT")
    require("--add busybox,proot,ca-certificates,termux-api" in BUILDER, "TERMUX_API_PACKAGE_NOT_EMBEDDED")
    for token in [
        '"bin/termux-battery-status"',
        '"bin/termux-sensor"',
        '"libexec/termux-api"',
        '"libexec/termux-api-broadcast"',
        "BOOTSTRAP_TERMUX_API_CLI",
        "termux_api_cli=EMBEDDED",
        "Package: termux-api",
        "API_RECEIVER_COMPONENT",
        "etc/apt/sources.list.d/termux.sources",
        "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
        "Enabled: no",
        '"runtime_materialized": False',
        '"RAFCODEPHI_RUNTIME_MATERIALIZED": "0"',
    ]:
        require(token in BUILDER, f"BUILDER_TOKEN_MISSING:{token}")
    require("device_runtime_proof=TOKEN_VAZIO" in BUILDER, "DEVICE_PROOF_BOUNDARY_MISSING")
    require("claim_allowed_device_runtime=false" in BUILDER, "CLAIM_BOUNDARY_MISSING")
    # Exercise the same sed token substitution used by the actual package patcher.
    # Static token presence alone did not protect the old 95-minute build.
    target_prefix = "/data/data/com.termux.rafacodephi/files/usr"
    result = subprocess.run(
        ["sed", "-e", r"s%\@TERMUX_PREFIX\@%" + target_prefix + "%g"],
        input=PATCH, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=True,
    )
    added = "\n".join(
        line[1:] for line in result.stdout.splitlines()
        if line.startswith("+") and not line.startswith("+++")
    )
    require(
        f'# define PREFIX "{target_prefix}"' in added,
        "PREFIX_PATCH_RUNTIME_SUBSTITUTION_FAILED",
    )
    require("@TERMUX_PREFIX@" not in added, "PREFIX_PATCH_TOKEN_UNRESOLVED")
    require("/data/data/com.termux/files/usr" not in added,
            "PREFIX_PATCH_LEGACY_PREFIX_IN_ADDITIONS")
    require(f'child_argv[5] = "{TARGET}";' in added,
            "API_RECEIVER_RUNTIME_PATCH_MISSING")
    print(
        "RAFCODEPHI_TERMUX_API_CONTRACT=PASS "
        f"receiver={TARGET} prefix_template=true embedded_cli=true device_runtime_proof=TOKEN_VAZIO claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
