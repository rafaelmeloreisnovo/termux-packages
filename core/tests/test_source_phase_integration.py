#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "termux-build-core.c").read_text(encoding="utf-8")
SOURCE = (ROOT / "source_download.c").read_text(encoding="utf-8")
HEADER = (ROOT / "source_download.h").read_text(encoding="utf-8")
MAKEFILE = (ROOT / "Makefile").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    require('#include "source_download.h"' in CORE,
            "production build core does not include source acquisition contract")
    require('return termux_acquire_source(ctx);' in CORE,
            "get-source phase does not delegate missing source trees to the acquisition primitive")
    require('mode=PREMATERIALIZED' in CORE,
            "pre-materialized compatibility path was not preserved")
    require('mode=MANIFEST_ACQUIRE' in CORE,
            "manifest acquisition path is not observable")
    require('int termux_acquire_source(struct termux_build_context *ctx)' in HEADER,
            "source acquisition API declaration missing")
    require('int termux_acquire_source(struct termux_build_context *ctx)' in SOURCE,
            "source acquisition implementation missing")
    require('int termux_phase_get_source(struct termux_build_context *ctx)' not in SOURCE,
            "regression: downloader owns a phase symbol instead of the acquisition primitive")
    require('source_download.o' in MAKEFILE.split('termux-build-core:', 1)[1].split('\n', 1)[0],
            "production executable is not linked with source_download.o")

    print("SOURCE_PHASE_INTEGRATION_CONTRACT=PASS claim_allowed=false runtime_download=NOT_MEASURED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
