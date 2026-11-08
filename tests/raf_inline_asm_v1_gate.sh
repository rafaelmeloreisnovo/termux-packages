#!/bin/sh
set -eu
# First argument: header directory relative to the repository root.
INC=$1
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=cc
CLANG=clang
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
# Cross-ISA validation must disassemble the compiled ELF object, not source-only ASM.
LLVM_DISASM=${LLVM_OBJDUMP:-}
if [ -z "$LLVM_DISASM" ]; then
  for tool in llvm-objdump llvm-objdump-21 llvm-objdump-20 llvm-objdump-19 llvm-objdump-18 llvm-objdump-17 llvm-objdump-16; do
    if command -v "$tool" >/dev/null 2>&1; then
      LLVM_DISASM=$(command -v "$tool")
      break
    fi
  done
fi
if [ -z "$LLVM_DISASM" ] || ! command -v "$LLVM_DISASM" >/dev/null 2>&1; then
  echo 'BLOCKED: LLVM cross-ISA disassembler missing (no source-only PASS fallback)' >&2
  exit 127
fi
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
  -I"$ROOT/$INC" -DRAF_ASM_HOST_MAIN "$ROOT/tests/raf_inline_asm_v1_selftest.c" -o "$TMP/host"
"$TMP/host"
sed 's/0x78563412u/0x78563413u/' "$ROOT/tests/raf_inline_asm_v1_selftest.c" > "$TMP/invalid.c"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$ROOT/$INC" \
  -DRAF_ASM_HOST_MAIN "$TMP/invalid.c" -o "$TMP/invalid"
if "$TMP/invalid"; then echo 'FAIL: deliberate bad vector was accepted' >&2; exit 1; fi
for target in armv7-thumb armv7-a32 aarch64; do
  case "$target" in
    armv7-thumb) set -- --target=armv7a-linux-gnueabi -march=armv7-a -mthumb ;;
    armv7-a32) set -- --target=armv7a-linux-gnueabi -march=armv7-a -marm ;;
    aarch64) set -- --target=aarch64-linux-gnu ;;
  esac
  "$CLANG" "$@" -std=c11 -O2 -ffreestanding -fno-builtin \
    -fno-stack-protector -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
    -I"$ROOT/$INC" -c "$ROOT/tests/raf_inline_asm_v1_selftest.c" -o "$TMP/$target.o"
  if [ -n "$(nm -u "$TMP/$target.o")" ]; then
    echo "FAIL: unresolved symbols for $target" >&2; exit 1
  fi
  "$LLVM_DISASM" -d "$TMP/$target.o" > "$TMP/$target.asm"
  for instruction in rev clz rbit; do
    if ! grep -Eq "[[:space:]]$instruction([.][a-z0-9]+)?[[:space:]]" "$TMP/$target.asm"; then
      echo "FAIL: $instruction not emitted for $target" >&2; exit 1
    fi
  done
  echo "$target: object + ISA + no-undefined PASS"
done
printf '%s\n' 'HOST: vectors + adversarial negative PASS' \
  'ARMv7-A32/Thumb2/AArch64: COMPILE_ONLY PASS; physical execution NOT_RUN'
