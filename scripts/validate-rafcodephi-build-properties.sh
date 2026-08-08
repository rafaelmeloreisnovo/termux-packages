#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# shellcheck source=properties.sh
source scripts/properties.sh

fail() {
  printf 'RAFCODEPHI_BUILD_PROPERTIES_RUNTIME=FAIL reason=%s\n' "$1" >&2
  exit 1
}

[[ "${TERMUX_APP__PACKAGE_NAME:-}" == "com.termux.rafacodephi" ]] || fail "APP_PACKAGE=${TERMUX_APP__PACKAGE_NAME:-UNSET}"
[[ "${TERMUX_APP__DATA_DIR:-}" == "/data/data/com.termux.rafacodephi" ]] || fail "APP_DATA_DIR=${TERMUX_APP__DATA_DIR:-UNSET}"
[[ "${TERMUX__ROOTFS:-}" == "/data/data/com.termux.rafacodephi/files" ]] || fail "ROOTFS=${TERMUX__ROOTFS:-UNSET}"
[[ "${TERMUX__PREFIX:-}" == "/data/data/com.termux.rafacodephi/files/usr" ]] || fail "INTERNAL_PREFIX=${TERMUX__PREFIX:-UNSET}"
[[ "${TERMUX_PREFIX:-}" == "/data/data/com.termux.rafacodephi/files/usr" ]] || fail "PUBLIC_PREFIX=${TERMUX_PREFIX:-UNSET}"

# Preserve the official Termux binary-repository identity until a separately
# validated RAFCODEPhi binary repository exists. This makes incompatible repo
# artifacts fail compatibility checks instead of being silently reused.
[[ "${TERMUX_REPO_APP__PACKAGE_NAME:-}" == "com.termux" ]] || fail "REPO_APP_PACKAGE=${TERMUX_REPO_APP__PACKAGE_NAME:-UNSET}"
[[ "${TERMUX_REPO__PREFIX:-}" == "/data/data/com.termux/files/usr" ]] || fail "REPO_PREFIX=${TERMUX_REPO__PREFIX:-UNSET}"
[[ "$TERMUX_PREFIX" != "$TERMUX_REPO__PREFIX" ]] || fail "CURRENT_AND_REPO_PREFIX_MUST_DIFFER"

printf '%s\n' \
  'RAFCODEPHI_BUILD_PROPERTIES_RUNTIME=PASS' \
  "APP_PACKAGE=$TERMUX_APP__PACKAGE_NAME" \
  "APP_DATA_DIR=$TERMUX_APP__DATA_DIR" \
  "ROOTFS=$TERMUX__ROOTFS" \
  "PREFIX=$TERMUX_PREFIX" \
  "REPO_APP_PACKAGE=$TERMUX_REPO_APP__PACKAGE_NAME" \
  "REPO_PREFIX=$TERMUX_REPO__PREFIX" \
  'REPO_BINARY_REUSE_COMPATIBLE=false' \
  'CLAIM_ALLOWED=false' \
  'RELEASE_ALLOWED=false'
