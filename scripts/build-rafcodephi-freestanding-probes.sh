#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${RAFCODEPHI_FREESTANDING_OUT:-$ROOT/artifacts/rafcodephi-freestanding}"
CC="${CLANG:-clang}"
mkdir -p "$OUT"

"$CC" --target=armv7a-linux-androideabi21 \
  -nostdlib -static -fuse-ld=lld \
  -Wl,-e,_start -Wl,--build-id=none -Wl,-s -Wl,-z,noexecstack \
  -o "$OUT/rafcodephi-presence16-arm" \
  "$ROOT/scripts/rafcodephi-freestanding/rafcodephi_presence16_arm32.S"

"$CC" --target=aarch64-linux-android21 \
  -nostdlib -static -fuse-ld=lld \
  -Wl,-e,_start -Wl,--build-id=none -Wl,-s -Wl,-z,noexecstack \
  -o "$OUT/rafcodephi-presence16-aarch64" \
  "$ROOT/scripts/rafcodephi-freestanding/rafcodephi_presence16_aarch64.S"

! readelf -d "$OUT/rafcodephi-presence16-arm" | grep -q NEEDED
! readelf -d "$OUT/rafcodephi-presence16-aarch64" | grep -q NEEDED
! readelf -S "$OUT/rafcodephi-presence16-arm" | grep -Eq '\.(dynsym|symtab)'
! readelf -S "$OUT/rafcodephi-presence16-aarch64" | grep -Eq '\.(dynsym|symtab)'
! grep -Eq '^[[:space:]]*(bl|blx|ret)[[:space:]]' "$ROOT/scripts/rafcodephi-freestanding/rafcodephi_presence16_arm32.S"
! grep -Eq '^[[:space:]]*(bl|blr|ret)[[:space:]]' "$ROOT/scripts/rafcodephi-freestanding/rafcodephi_presence16_aarch64.S"

printf 'freestanding=1\nlibc=0\nheap=0\ngc=0\ndynamic=0\nsymbol_table=0\nloop=0\ntail_call=0\nparallel_lanes=16\nhashing_gate=0\narm_bytes=%s\naarch64_bytes=%s\n' \
  "$(wc -c < "$OUT/rafcodephi-presence16-arm" | tr -d ' ')" \
  "$(wc -c < "$OUT/rafcodephi-presence16-aarch64" | tr -d ' ')" \
  > "$OUT/RAFCODEPHI_FREESTANDING16.manifest"
