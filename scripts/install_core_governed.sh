#!/usr/bin/env bash
# Governed RAFCODEPHI core installation surface.
#
# This intentionally does NOT call `make install`, because the legacy Makefile
# target depends on the broad regression TARGETS set. Only binaries listed in
# core/product_surface.v1.json are build/install eligible here.
#
# This script installs into PREFIX (or DESTDIR+PREFIX) but does not claim an
# Android/device runtime. The caller must produce a separate tree/artifact receipt.

set -euo pipefail

REPO_ROOT="${REPO_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
CORE_DIR="$REPO_ROOT/core"
SURFACE="$CORE_DIR/product_surface.v1.json"
PREFIX="${PREFIX:-/data/data/com.termux.rafacodephi/files/usr}"
DESTDIR="${DESTDIR:-}"

fail() {
    printf 'GOVERNED_INSTALL=BLOCKED reason=%s\n' "$*" >&2
    printf 'CLAIM_ALLOWED=false\n' >&2
    exit 1
}

for cmd in jq make install sha256sum; do
    command -v "$cmd" >/dev/null 2>&1 || fail "required command missing: $cmd"
done
[ -f "$SURFACE" ] || fail "product surface missing: $SURFACE"
jq -e '.schema == "rafcodephi_core_product_surface/1.0.0" and .claim_allowed == false' "$SURFACE" >/dev/null \
    || fail "invalid product surface contract"

mapfile -t ALLOWED < <(jq -r '.allowed_binaries[]' "$SURFACE")
[ "${#ALLOWED[@]}" -gt 0 ] || fail "allowed_binaries is empty"

# Fail closed if the declared list changes to include a test/fixture name.
for bin in "${ALLOWED[@]}"; do
    case "$bin" in
        test-*|device-test-*|*benchmark*|*fixture*)
            fail "test/fixture binary cannot enter product surface: $bin"
            ;;
    esac
done

printf 'GOVERNED_INSTALL_SOURCE=core/product_surface.v1.json\n'
printf 'PREFIX=%s\n' "$PREFIX"
printf 'DESTDIR=%s\n' "${DESTDIR:-<empty>}"
printf 'BUILD_TARGETS=%s\n' "${ALLOWED[*]}"

make -C "$CORE_DIR" "${ALLOWED[@]}"

BIN_DIR="${DESTDIR}${PREFIX}/bin"
install -d "$BIN_DIR"

for bin in "${ALLOWED[@]}"; do
    src="$CORE_DIR/$bin"
    [ -x "$src" ] || fail "expected executable missing after build: $src"
    install -m 755 "$src" "$BIN_DIR/$bin"
    printf 'INSTALLED=%s sha256=%s\n' \
        "$BIN_DIR/$bin" "$(sha256sum "$BIN_DIR/$bin" | awk '{print $1}')"
done

# Prove exact file-name surface under the bin directory when DESTDIR is supplied
# for an isolated receipt. On a live prefix we do not assume ownership of other
# unrelated files.
if [ -n "$DESTDIR" ]; then
    mapfile -t actual < <(find "$BIN_DIR" -maxdepth 1 -type f -printf '%f\n' | sort)
    mapfile -t expected < <(printf '%s\n' "${ALLOWED[@]}" | sort)
    [ "${actual[*]}" = "${expected[*]}" ] \
        || fail "isolated install surface differs from allowed_binaries"
fi

printf 'GOVERNED_INSTALL=PASS\n'
printf 'PRODUCT_SURFACE=SOURCE_INSTALL_TREE_ONLY\n'
printf 'ANDROID_RUNTIME=NOT_MEASURED\n'
printf 'CLAIM_ALLOWED=false\n'
