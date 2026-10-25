#!/usr/bin/env bash
# CI-only NDK prerequisite; builder container, never the Android runtime.
# Google NDK archive digest is defined in upstream scripts/setup-android-sdk.sh.
set -euo pipefail
cd /home/builder/termux-packages
# The image owns /home/builder/lib (not writable by the builder user).
# Keep SHA-verified NDK29 in the already writable checked-out workspace.
mkdir -p build-tools
[[ -w build-tools ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=builder_workspace_not_writable' >&2
  exit 2
}
export NDK="$PWD/build-tools/android-ndk-r29"
source scripts/properties.sh
[[ "$NDK" == "$PWD/build-tools/android-ndk-r29" ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=ndk_location_overridden' >&2
  exit 2
}
[[ "${TERMUX_NDK_VERSION:-}" == 29 ]] || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=unexpected_ndk_version' >&2
  exit 2
}
ndk_sha='4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf'
grep -Eq "^[[:space:]]*ANDROID_NDK_SHA256=${ndk_sha}$" scripts/setup-android-sdk.sh || {
  echo 'RAFCODEPHI_NDK29=BLOCKED reason=unreviewed_source_archive_sha' >&2
  exit 2
}
if [[ ! -d "$NDK" ]]; then
  echo 'RAFCODEPHI_NDK29=PROVISIONING via_verified_upstream_installer=true'
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
