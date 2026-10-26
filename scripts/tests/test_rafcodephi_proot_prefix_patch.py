#!/usr/bin/env python3
"""Contract for the scoped PRoot diagnostic-string source patch.

The full ARM/AArch64 build is the artifact gate. This fast test only proves that
the recipe patch is narrow, keeps the pinned upstream source, and removes the
observed non-functional legacy marker without weakening the ZIP/DEB scanners.
"""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "packages/proot/build.sh"
PATCH = ROOT / "packages/proot/src-cli-cli.c.patch"

OLD = (
    '"It seems that termux-exec is active and is prepending '
    '/data/data/com.termux/... to executable paths\\n"'
)
NEW = (
    '"It seems that termux-exec is active and is prepending the host app data '
    'directory to executable paths\\n"'
)


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise SystemExit("RAFCODEPHI_PROOT_PREFIX_PATCH=BLOCKED reason=" + reason)


def main() -> int:
    build = BUILD.read_text(encoding="utf-8")
    patch = PATCH.read_text(encoding="utf-8")

    require(
        "https://github.com/termux/proot/archive/v${TERMUX_PKG_VERSION}.zip"
        in build,
        "UPSTREAM_SOURCE_ROUTE_CHANGED",
    )
    require(
        "TERMUX_PKG_SHA256=d237b21b6d84a3acb00507f96251dcf2bbfbee9ffc66ab6258a3f86ef7874186"
        in build,
        "UPSTREAM_SOURCE_SHA_CHANGED",
    )
    require(patch.count("-" + OLD) == 1, "LEGACY_DIAGNOSTIC_NOT_REMOVED_ONCE")
    require(patch.count("+" + NEW) == 1, "NEUTRAL_DIAGNOSTIC_NOT_ADDED_ONCE")

    changed = [
        line for line in patch.splitlines()
        if line[:1] in ("+", "-") and not line.startswith(("+++", "---"))
    ]
    require(changed == ["-" + OLD, "+" + NEW], "PATCH_SCOPE_EXCEEDS_ONE_STRING")
    require("/data/data/com.termux.rafacodephi" not in patch,
            "PATCH_HARDCODES_SUCCESSOR_PACKAGE")

    print(
        "RAFCODEPHI_PROOT_PREFIX_PATCH=PASS_SOURCE_ONLY "
        "logic_changed=false artifact_build=NOT_RUN claim_allowed=false"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
