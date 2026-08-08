#!/usr/bin/env python3
"""Static fail-closed contract for the embedded RAFCODEPHI Termux:API CLI."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATCH = (ROOT / "packages/termux-api/termux-api.c.patch").read_text(encoding="utf-8")
BUILDER = (ROOT / "scripts/build-rafcodephi-real-bootstrap.sh").read_text(encoding="utf-8")
TARGET = "com.termux.rafacodephi.api/com.termux.api.TermuxApiReceiver"


def require(condition: bool, token: str) -> None:
    if not condition:
        raise SystemExit(f"RAFCODEPHI_TERMUX_API_CONTRACT=BLOCKED reason={token}")


def main() -> int:
    require(f'+    child_argv[5] = "{TARGET}";' in PATCH, "API_RECEIVER_TARGET_MISSING")
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
    ]:
        require(token in BUILDER, f"BUILDER_TOKEN_MISSING:{token}")
    require("device_runtime_proof=TOKEN_VAZIO" in BUILDER, "DEVICE_PROOF_BOUNDARY_MISSING")
    require("claim_allowed_device_runtime=false" in BUILDER, "CLAIM_BOUNDARY_MISSING")
    print(
        "RAFCODEPHI_TERMUX_API_CONTRACT=PASS "
        f"receiver={TARGET} embedded_cli=true device_runtime_proof=TOKEN_VAZIO claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
