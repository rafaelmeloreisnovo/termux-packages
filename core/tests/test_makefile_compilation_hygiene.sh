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

all_line() {
  local cc="$1"
  shift
  env "$@" make -s -pn CC="$cc" all 2>/dev/null | grep -m1 '^all:'
}

install_line() {
  local cc="$1"
  make -s -pn CC="$cc" install 2>/dev/null | grep -m1 '^install:'
}

ARM_ALL="$(all_line "$TMP/cc-arm")"
A64_ALL="$(all_line "$TMP/cc-aarch64")"
X86_ALL="$(all_line "$TMP/cc-x86")"
TERMUX_ALL="$(all_line "$TMP/cc-arm" TERMUX_PREFIX=/data/data/com.termux.rafacodephi/files/usr)"
INSTALL_LINE="$(install_line "$TMP/cc-arm")"

# Non-x86 default graphs must never request the explicit AVX-512 targets.
[[ "$ARM_ALL" != *advanced-simd-benchmark* ]]
[[ "$ARM_ALL" != *advanced-simd-test* ]]
[[ "$A64_ALL" != *advanced-simd-benchmark* ]]
[[ "$A64_ALL" != *advanced-simd-test* ]]

# Preserve the full host graph on x86 so the overlay does not silently weaken it.
[[ "$X86_ALL" == *advanced-simd-benchmark* ]]
[[ "$X86_ALL" == *advanced-simd-test* ]]

# A Termux packaging build and make install should build exactly the artifacts
# that the legacy install recipe actually copies into PREFIX.
for target in termux-build-core test-manifest manifest-dumper; do
  [[ "$TERMUX_ALL" == *"$target"* ]]
  [[ "$INSTALL_LINE" == *"$target"* ]]
done
for forbidden in advanced-simd-benchmark advanced-simd-test pkg-real metrics-producer; do
  [[ "$TERMUX_ALL" != *"$forbidden"* ]]
  [[ "$INSTALL_LINE" != *"$forbidden"* ]]
done

# Reproduce the stale-object class that previously survived `make clean`.
touch stale-cross-compiler.o pkg_real_cli.o tests/stale-cross-compiler.o tests/test_pkg_real.o
make -s clean
for stale in stale-cross-compiler.o pkg_real_cli.o tests/stale-cross-compiler.o tests/test_pkg_real.o; do
  if [[ -e "$stale" ]]; then
    printf 'FAIL: clean left stale object: %s\n' "$stale" >&2
    exit 1
  fi
done

printf '%s\n' 'COMPILATION_HYGIENE_PASS'
