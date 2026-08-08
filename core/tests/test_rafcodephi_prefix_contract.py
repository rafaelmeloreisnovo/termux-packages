#!/usr/bin/env python3
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "build_exec.c").read_text(encoding="utf-8")
TARGET = "/data/data/com.termux.rafacodephi/files/usr"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    require(
        f'#define RAFCODEPHI_TARGET_PREFIX "{TARGET}"' in SOURCE,
        "canonical RAFCODEPhi target prefix constant is missing",
    )
    require(
        "RAFCODEPHI_TARGET_PREFIX, RAFCODEPHI_TARGET_PREFIX" in SOURCE,
        "configure branches are not both bound to RAFCODEPHI_TARGET_PREFIX",
    )
    require(
        "make install DESTDIR='%s'" in SOURCE,
        "DESTDIR staging contract is missing",
    )
    require(
        "staging_model=DESTDIR claim_allowed=false" in SOURCE,
        "configure receipt boundary is missing",
    )

    configure_region = SOURCE[
        SOURCE.index("int termux_exec_configure") : SOURCE.index("int termux_exec_make(")
    ]
    require(
        "--prefix='%s'" in configure_region,
        "configure command does not set --prefix",
    )
    require(
        not re.search(
            r"abs_build\s*,\s*abs_source\s*,\s*abs_source\s*,\s*abs_build\s*,\s*abs_build",
            configure_region,
        ),
        "regression: build directory is still being used as runtime --prefix",
    )
    require(
        "/data/data/com.termux/files/usr" not in SOURCE,
        "legacy upstream runtime prefix leaked into build_exec.c",
    )

    print(
        "RAFCODEPHI_PREFIX_SOURCE_CONTRACT=PASS "
        f"target_prefix={TARGET} staging=DESTDIR claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
