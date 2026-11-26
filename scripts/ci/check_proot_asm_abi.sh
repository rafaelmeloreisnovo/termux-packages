#!/usr/bin/env bash
# SOURCE-ONLY acceptance; never runs ptrace or replaces upstream PRoot.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CC="${CLANG:-clang}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
for arch in armv7a-linux-androideabi21 aarch64-linux-android21; do
  flags=()
  hex=010000d4
  if [[ "$arch" == armv7* ]]; then
    flags=(-marm)
    hex=000000ef
  fi
  "$CC" --target="$arch" "${flags[@]}" -std=c11 -Oz -Wall -Wextra -Werror -Wpedantic \
    -ffreestanding -fno-builtin -fno-stack-protector -fno-pic \
    -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -c "$ROOT/scripts/rafcodephi-freestanding/raf_proot_rawptrace.c" -o "$TMP/a.o"
  test -z "$(nm -u "$TMP/a.o")"
  readelf -x .text "$TMP/a.o" | grep -qi "$hex"
  "$CC" --target="$arch" "${flags[@]}" -nostdlib -static -fuse-ld=lld \
    -Wl,-e,raf_proot_raw_ptrace -Wl,--build-id=none -Wl,-z,noexecstack \
    "$TMP/a.o" -o "$TMP/a.elf"
  readelf -r "$TMP/a.elf" | grep -q 'There are no relocations'
  readelf -d "$TMP/a.elf" | grep -q 'There is no dynamic section'
  test "$(readelf -s "$TMP/a.elf" | grep -c 'raf_proot_raw_ptrace')" -eq 1
  sha256sum "$TMP/a.elf"
  echo "$arch SOURCE_SCOPED_LINK_PASS, RUNTIME=NOT_RUN"
done
