#!/usr/bin/env bash
# CI-only read-only prerequisite gate inside the canonical package builder.
# Not an Android runtime or an NDK installer; never claims build success.
set -euo pipefail
cd /home/builder/termux-packages
source scripts/properties.sh
if [[ "${TERMUX_NDK_VERSION:-}" != "29" ]]; then
  printf 'RAFCODEPHI_NDK_PREFLIGHT=BLOCKED reason=NDK_VERSION_UNEXPECTED expected=29 actual=%s\n' "${TERMUX_NDK_VERSION:-TOKEN_VAZIO}" >&2
  exit 2
fi
if [[ -z "${NDK:-}" || ! -d "$NDK" ]]; then
  printf 'RAFCODEPHI_NDK_PREFLIGHT=BLOCKED reason=NDK_MISSING ndk=%s\n' "${NDK:-TOKEN_VAZIO}" >&2
  exit 3
fi
if [[ ! -r "$NDK/source.properties" ]]; then
  echo 'RAFCODEPHI_NDK_PREFLIGHT=BLOCKED reason=NDK_METADATA_MISSING' >&2
  exit 4
fi
if ! grep -Eq '^[[:space:]]*Pkg\.Revision[[:space:]]*=[[:space:]]*29\.' "$NDK/source.properties"; then
  echo 'RAFCODEPHI_NDK_PREFLIGHT=BLOCKED reason=NDK_REVISION_MISMATCH' >&2
  exit 5
fi
if [[ ! -x "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" ]]; then
  echo 'RAFCODEPHI_NDK_PREFLIGHT=BLOCKED reason=NDK_COMPILER_MISSING' >&2
  exit 6
fi
printf 'RAFCODEPHI_NDK_PREFLIGHT=PASS_SCOPED ndk_revision=29 builder_only=true claim_allowed=false\n'
