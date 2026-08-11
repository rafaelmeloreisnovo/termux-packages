#!/data/data/com.termux.rafacodephi/files/usr/bin/sh
set -eu

OUT="${1:-rafcodephi-d10-device-receipt.json}"
PREFIX_EXPECTED="${PREFIX_EXPECTED:-/data/data/com.termux.rafacodephi/files/usr}"
PACKAGE_EXPECTED="${PACKAGE_EXPECTED:-com.termux.rafacodephi}"

fail() {
  code="$1"
  msg="$2"
  printf '%s\n' "D10_BLOCKED: $code: $msg" >&2
  exit "$code"
}

command -v uname >/dev/null 2>&1 || fail 20 "uname unavailable"
command -v sha256sum >/dev/null 2>&1 || fail 21 "sha256sum unavailable"
command -v getprop >/dev/null 2>&1 || fail 22 "getprop unavailable; physical Android evidence required"

ANDROID_RELEASE="$(getprop ro.build.version.release 2>/dev/null || true)"
ANDROID_SDK="$(getprop ro.build.version.sdk 2>/dev/null || true)"
BUILD_FINGERPRINT="$(getprop ro.build.fingerprint 2>/dev/null || true)"
DEVICE_PRODUCT="$(getprop ro.product.device 2>/dev/null || true)"
DEVICE_MODEL="$(getprop ro.product.model 2>/dev/null || true)"
ABI_PRIMARY="$(getprop ro.product.cpu.abi 2>/dev/null || true)"

[ -n "$ANDROID_RELEASE" ] || fail 23 "ro.build.version.release empty"
[ -n "$ANDROID_SDK" ] || fail 24 "ro.build.version.sdk empty"
[ -n "$BUILD_FINGERPRINT" ] || fail 25 "ro.build.fingerprint empty"
[ -n "$ABI_PRIMARY" ] || fail 26 "ro.product.cpu.abi empty"

PREFIX_ACTUAL="${PREFIX:-}"
[ -n "$PREFIX_ACTUAL" ] || fail 27 "PREFIX not set"
[ "$PREFIX_ACTUAL" = "$PREFIX_EXPECTED" ] || fail 28 "unexpected PREFIX: $PREFIX_ACTUAL"

case "$PREFIX_ACTUAL" in
  /data/data/"${PACKAGE_EXPECTED}"/files/usr|/data/user/0/"${PACKAGE_EXPECTED}"/files/usr) ;;
  *) fail 29 "PREFIX does not bind expected package id" ;;
esac

BIN_DIR="$PREFIX_ACTUAL/bin"
ARCH_PROBE="$BIN_DIR/arch-probe"
PKG_REAL="$BIN_DIR/pkg-real"
[ -x "$ARCH_PROBE" ] || fail 30 "arch-probe not installed/executable"
[ -x "$PKG_REAL" ] || fail 31 "pkg-real not installed/executable"

TMP_BASE="${TMPDIR:-$PREFIX_ACTUAL/tmp}/rafcodephi-d10-$$"
mkdir -p "$TMP_BASE"
trap 'rm -rf "$TMP_BASE"' EXIT HUP INT TERM

ARCH_JSON="$TMP_BASE/arch-probe.json"
"$ARCH_PROBE" "$ARCH_JSON"
[ -s "$ARCH_JSON" ] || fail 32 "arch-probe produced no receipt"

PKG_LOG="$TMP_BASE/pkg-real.log"
RC=0
"$PKG_REAL" --help >"$PKG_LOG" 2>&1 || RC=$?
[ "$RC" -le 2 ] || fail 33 "pkg-real runtime returned $RC"

ARCH_SHA="$(sha256sum "$ARCH_PROBE" | awk '{print $1}')"
PKG_SHA="$(sha256sum "$PKG_REAL" | awk '{print $1}')"
ARCH_RECEIPT_SHA="$(sha256sum "$ARCH_JSON" | awk '{print $1}')"
PKG_LOG_SHA="$(sha256sum "$PKG_LOG" | awk '{print $1}')"
FINGERPRINT_SHA="$(printf '%s' "$BUILD_FINGERPRINT" | sha256sum | awk '{print $1}')"
UNAME_M="$(uname -m)"
UNAME_S="$(uname -s)"
UTC="$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || printf 'TOKEN_VAZIO')"

# Values included directly below are constrained to Android property/uname/path tokens;
# the full build fingerprint itself is deliberately represented only by SHA-256.
{
  printf '{\n'
  printf '  "schema":"rafcodephi_d10_physical_device/1.0.0",\n'
  printf '  "status":"D10_PHYSICAL_DEVICE_PASS",\n'
  printf '  "claim_allowed":false,\n'
  printf '  "runtime_kind":"ANDROID_TERMUX_PHYSICAL",\n'
  printf '  "android_bionic_runtime":"MEASURED_ON_DEVICE",\n'
  printf '  "physical_android":"PASS",\n'
  printf '  "persistent_trust_root":"TOKEN_VAZIO",\n'
  printf '  "package_id":"%s",\n' "$PACKAGE_EXPECTED"
  printf '  "prefix":"%s",\n' "$PREFIX_ACTUAL"
  printf '  "android_release":"%s",\n' "$ANDROID_RELEASE"
  printf '  "android_sdk":"%s",\n' "$ANDROID_SDK"
  printf '  "device_product":"%s",\n' "$DEVICE_PRODUCT"
  printf '  "device_model":"%s",\n' "$DEVICE_MODEL"
  printf '  "abi_primary":"%s",\n' "$ABI_PRIMARY"
  printf '  "uname_s":"%s",\n' "$UNAME_S"
  printf '  "uname_m":"%s",\n' "$UNAME_M"
  printf '  "observed_at_utc":"%s",\n' "$UTC"
  printf '  "sha256":{\n'
  printf '    "build_fingerprint":"%s",\n' "$FINGERPRINT_SHA"
  printf '    "arch_probe_binary":"%s",\n' "$ARCH_SHA"
  printf '    "pkg_real_binary":"%s",\n' "$PKG_SHA"
  printf '    "arch_probe_receipt":"%s",\n' "$ARCH_RECEIPT_SHA"
  printf '    "pkg_real_log":"%s"\n' "$PKG_LOG_SHA"
  printf '  },\n'
  printf '  "rule":"PHYSICAL_PASS_REQUIRES_THIS_RECEIPT_FROM_THE_DEVICE_ITSELF"\n'
  printf '}\n'
} > "$OUT"

RECEIPT_SHA="$(sha256sum "$OUT" | awk '{print $1}')"
printf '%s  %s\n' "$RECEIPT_SHA" "$OUT" > "$OUT.sha256"
printf '%s\n' "D10_PHYSICAL_DEVICE_PASS receipt=$OUT sha256=$RECEIPT_SHA"
