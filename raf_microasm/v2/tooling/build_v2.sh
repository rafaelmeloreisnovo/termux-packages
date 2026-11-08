#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST=${1:-"$ROOT/out"}
mkdir -p "$DEST"
CC_HOST=${CC_HOST:-cc}
CLANG=${CLANG:-clang}
AR_BIN=${RAF_AR:-}
if [ -z "$AR_BIN" ]; then
  for candidate in llvm-ar llvm-ar-22 llvm-ar-21 llvm-ar-20 llvm-ar-19 llvm-ar-18 llvm-ar-17 ar; do
    if command -v "$candidate" >/dev/null 2>&1; then AR_BIN=$candidate; break; fi
  done
fi
[ -n "$AR_BIN" ] && command -v "$AR_BIN" >/dev/null 2>&1 || { echo "missing LLVM/GNU archiver" >&2; exit 127; }
OBJDUMP_BIN=${RAF_OBJDUMP:-}
if [ -z "$OBJDUMP_BIN" ]; then
  for candidate in llvm-objdump llvm-objdump-22 llvm-objdump-21 llvm-objdump-20 llvm-objdump-19 llvm-objdump-18 llvm-objdump-17; do
    if command -v "$candidate" >/dev/null 2>&1; then OBJDUMP_BIN=$candidate; break; fi
  done
fi
[ -n "$OBJDUMP_BIN" ] && command -v "$OBJDUMP_BIN" >/dev/null 2>&1 || { echo "missing LLVM cross-ISA disassembler" >&2; exit 127; }
# closed target/flag vocabulary: no eval; no external imports in compiled leaves
n=0
while IFS='|' read -r target rel opt; do
  case "$target" in ''|'#'*) continue;; esac
  case "$opt" in -Os|-O2|-O3) ;; *) echo "bad optimization: $opt" >&2; exit 2;; esac
  case "$target" in
    portable) c=$CC_HOST; set -- ;;
    armv7-a32) c=$CLANG; set -- --target=armv7a-linux-androideabi21 -march=armv7-a -marm ;;
    armv7-thumb2) c=$CLANG; set -- --target=armv7a-linux-androideabi21 -march=armv7-a -mthumb ;;
    aarch64) c=$CLANG; set -- --target=aarch64-linux-android21 -march=armv8-a ;;
    *) echo "unsupported target: $target" >&2; exit 2;;
  esac
  case "$rel" in
    src/portable/bits/*.c|src/armv7/bits/*.c|src/aarch64/bits/*.c) ;;
    *) echo "unsafe source path: $rel" >&2; exit 2;;
  esac
  case "$target:$rel" in
    portable:src/portable/*|armv7-a32:src/armv7/*|armv7-thumb2:src/armv7/*|aarch64:src/aarch64/*) ;;
    *) echo "architecture/source mismatch $target:$rel" >&2; exit 2;;
  esac
  base=${rel##*/}; op=${base%.c}
  case "$op" in rev32|rev16|rbit32|clz32|ctz32|ror32|lsl32|lsr32|xor32|and32|orr32|bic32) ;;
    *) echo "unknown primitive $op" >&2; exit 2;; esac
  mkdir -p "$DEST/$target"
  "$c" "$@" "$opt" -std=c11 -ffreestanding -fPIC -fno-builtin -fno-stack-protector \
    -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wpedantic \
    -I"$ROOT/include" -c "$ROOT/$rel" -o "$DEST/$target/$op.o"
  if [ -n "$(nm -u "$DEST/$target/$op.o")" ]; then
    echo "unresolved import: $target/$op" >&2; exit 3
  fi
  n=$((n+1))
done < "$ROOT/flags/units.v2.tsv"
[ "$n" -eq 48 ] || { echo "unexpected compiled units: $n" >&2; exit 4; }
for target in portable armv7-a32 armv7-thumb2 aarch64; do
  set -- "$DEST/$target/"*.o
  [ "$#" -eq 12 ] || { echo "unexpected object count: $target" >&2; exit 4; }
  "$AR_BIN" rcs "$DEST/$target/librafmicroasm.a" "$@"
done
# Host executable links portable archive. Any failed vector is a nonzero exit.
"$CC_HOST" -std=c11 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/tests/vectors.c" "$DEST/portable/librafmicroasm.a" -o "$DEST/host-vectors"
"$DEST/host-vectors"
"$CC_HOST" -std=c11 -O2 -Wall -Wextra -Werror -DRAFOP_NEGATIVE \
   -I"$ROOT/include" "$ROOT/tests/vectors.c" "$DEST/portable/librafmicroasm.a" \
   -o "$DEST/host-bad"
if "$DEST/host-bad"; then
   echo "negative vector unexpectedly passed" >&2; exit 5
fi
# Object-only ISA evidence. Linking into Android APK is explicitly NOT done.
for pair in 'rev32:rev' 'rev16:rev16' 'rbit32:rbit' 'clz32:clz' 'ctz32:rbit' \
 'ror32:ror' 'lsl32:lsl' 'lsr32:lsr' 'xor32:eor' 'and32:and' 'orr32:orr' 'bic32:bic'; do
  op=${pair%%:*}; ins=${pair#*:}
  for target in armv7-a32 armv7-thumb2 aarch64; do
    "$OBJDUMP_BIN" -d "$DEST/$target/$op.o" > "$DEST/$target/$op.dis"
    if ! grep -Eq "[[:space:]]$ins(\.w)?[[:space:]]" "$DEST/$target/$op.dis"; then
      echo "ISA instruction missing: $target/$op expected $ins" >&2; exit 6
    fi
  done
done
printf '%s\n' 'PASS_SCOPED: 48 separate ELF .o; 4 archives; 12 host primitive vectors + negative test; ISA evidence (3x12).' \
 'NOT_RUN: real ARM execution, APK, bootstrap, apt, dpkg, physical device.'
