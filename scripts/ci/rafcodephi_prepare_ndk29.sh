#!/usr/bin/env bash
# RAFAELIA CI-only prerequisite. Runs as builder user in canonical Docker.
# Reuses Termux's SDK installer with pinned NDK archive SHA256. No app mutation.
set -euo pipefail
cd /home/builder/termux-packages
source scripts/properties.sh
[[ "${TERMUX_NDK_VERSION:-}" == 29 ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=unexpected_ndk_version' >&2
  exit 2
}
grep -Eq '^[[:space:]]*ANDROID_NDK_SHA256=4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=unreviewed_source_archive_sha' >&2
  exit 2
}
if [[ ! -d "$NDK" ]]; then
  echo 'RAFCODEPHI_NDK29=PROVISIONING from_pinned_upstream_sdk_script=true'
  # setup-android-sdk.sh verifies vendor download SHA256; never use curl|sh.
  bash scripts/setup-android-sdk.sh
fi
[[ -r "$NDK/source.properties" ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=missing_metadata' >&2
  exit 3
}
grep -Eq '^[[:space:]]*Pkg\.Revision[[:space:]]*=[[:space:]]*29\.' "$NDK/source.properties" || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=version_mismatch' >&2
  exit 4
}
[[ -x "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=missing_clang' >&2
  exit 5
}
printf 'RAFCODEPHI_NDK29=PASS_SCOPED builder_only=true physical_android=NOT_RUN claim_allowed=false\n'
 scripts/setup-android-sdk.sh || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=unreviewed_source_archive_sha' >&2
  exit 2
}
if [[ ! -d "$NDK" ]]; then
  echo 'RAFCODEPHI_NDK29=PROVISIONING from_pinned_upstream_sdk_script=true'
  # setup-android-sdk.sh verifies vendor download SHA256; never use curl|sh.
  bash scripts/setup-android-sdk.sh
fi
[[ -r "$NDK/source.properties" ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=missing_metadata' >&2
  exit 3
}
grep -Eq '^[[:space:]]*Pkg\.Revision[[:space:]]*=[[:space:]]*29\.' "$NDK/source.properties" || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=version_mismatch' >&2
  exit 4
}
[[ -x "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=missing_clang' >&2
  exit 5
}
printf 'RAFCODEPHI_NDK29=PASS_SCOPED builder_only=true physical_android=NOT_RUN claim_allowed=false\n'
