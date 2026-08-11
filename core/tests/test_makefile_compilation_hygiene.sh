#!/usr/bin/env bash
set -euo pipefail

CORE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$CORE_DIR"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

make_fake_cc() {
  local path="$1" triple="$2"
  cat > "$path" <<EOF
#!/bin/sh
if [ "\${1:-}" = "-dumpmachine" ]; then
  printf '%s\n' '$triple'
  exit 0
fi
printf '%s\n' 'fake compiler invoked outside -dumpmachine probe' >&2
exit 99
EOF
  chmod +x "$path"
}

make_fake_cc "$TMP/cc-arm" 'arm-linux-gnueabihf'
make_fake_cc "$TMP/cc-aarch64" 'aarch64-linux-gnu'
make_fake_cc "$TMP/cc-x86" 'x86_64-linux-gnu'

target_line() {
  local target="$1" cc="$2"
  shift 2
  # Consume the complete `make -pn` stream.  `grep -m1` closes the pipe early,
  # which makes GNU make receive SIGPIPE and turns a successful graph probe into
  # a false RED when this script correctly runs with `set -o pipefail`.
  env "$@" make -s -pn CC="$cc" "$target" 2>/dev/null |
    awk -v key="$target:" '$1 == key { line=$0 } END { if (line == "") exit 1; print line }'
}

assert_contains() {
  local label="$1" haystack="$2" needle="$3"
  if [[ "$haystack" != *"$needle"* ]]; then
    printf 'FAIL[%s]: expected target %s\nGRAPH=%s\n' "$label" "$needle" "$haystack" >&2
    exit 1
  fi
}

assert_not_contains() {
  local label="$1" haystack="$2" needle="$3"
  if [[ "$haystack" == *"$needle"* ]]; then
    printf 'FAIL[%s]: forbidden target %s\nGRAPH=%s\n' "$label" "$needle" "$haystack" >&2
    exit 1
  fi
}

ARM_ALL="$(target_line all "$TMP/cc-arm")"
A64_ALL="$(target_line all "$TMP/cc-aarch64")"
X86_ALL="$(target_line all "$TMP/cc-x86")"
TERMUX_ALL="$(target_line all "$TMP/cc-arm" TERMUX_PREFIX=/data/data/com.termux.rafacodephi/files/usr)"
INSTALL_LINE="$(target_line install "$TMP/cc-arm")"

# Non-x86 default graphs must never request the explicit AVX-512 targets.
assert_not_contains ARM "$ARM_ALL" advanced-simd-benchmark
assert_not_contains ARM "$ARM_ALL" advanced-simd-test
assert_not_contains AARCH64 "$A64_ALL" advanced-simd-benchmark
assert_not_contains AARCH64 "$A64_ALL" advanced-simd-test

# Preserve the full host graph on x86 so the overlay does not silently weaken it.
assert_contains X86 "$X86_ALL" advanced-simd-benchmark
assert_contains X86 "$X86_ALL" advanced-simd-test

# A Termux packaging build and make install should build exactly the artifacts
# that the legacy install recipe actually copies into PREFIX.
for target in termux-build-core test-manifest manifest-dumper; do
  assert_contains TERMUX "$TERMUX_ALL" "$target"
  assert_contains INSTALL "$INSTALL_LINE" "$target"
done
for forbidden in advanced-simd-benchmark advanced-simd-test pkg-real metrics-producer; do
  assert_not_contains TERMUX "$TERMUX_ALL" "$forbidden"
  assert_not_contains INSTALL "$INSTALL_LINE" "$forbidden"
done

# Reproduce the stale-object class that previously survived `make clean`.
touch stale-cross-compiler.o pkg_real_cli.o tests/stale-cross-compiler.o tests/test_pkg_real.o
make -s clean
for stale in stale-cross-compiler.o pkg_real_cli.o tests/stale-cross-compiler.o tests/test_pkg_real.o; do
  if [[ -e "$stale" ]]; then
    printf 'FAIL[CLEAN]: clean left stale object: %s\n' "$stale" >&2
    exit 1
  fi
done

printf '%s\n' 'COMPILATION_HYGIENE_PASS'
