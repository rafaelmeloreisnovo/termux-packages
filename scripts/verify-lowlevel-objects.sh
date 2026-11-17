#!/usr/bin/env bash
set -euo pipefail
root=${1:-.}
out=${2:?usage: verify-lowlevel-objects.sh ROOT NEW_OUTPUT_DIRECTORY}
mkdir "$out"
out=$(cd "$out" && pwd)
cc=${CLANG:-clang}
"$cc" --version > "$out/compiler.txt"
bash "$root/scripts/verify-lowlevel-handoff.sh" "$root"
for abi in armeabi-v7a arm64-v8a; do
  case "$abi" in
    armeabi-v7a) arch=armv7; target=armv7a-linux-androideabi21 ;;
    arm64-v8a) arch=aarch64; target=aarch64-linux-android21 ;;
  esac
  for backend in portable "$arch"; do
    src="$root/raf_microasm/v2/src/$backend/bits/rbit32.c"
    flags=(--target="$target" -std=c11 -O2 -Wall -Wextra -Werror
      -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables
      -fno-asynchronous-unwind-tables -I "$root/raf_microasm/v2/include")
    "$cc" "${flags[@]}" -c "$src" -o "$out/$abi-$backend.o"
    "$cc" "${flags[@]}" -S "$src" -o "$out/$abi-$backend.s"
    # Scoped to these tiny pure primitives. A general syscall audit requires
    # disassembly and control-flow review of the final linked executable too.
    if grep -Eq '^[[:space:]]*(svc|swi|syscall|sysenter|bl|blx|blr)[[:space:]]' "$out/$abi-$backend.s"; then
      echo 'UNEXPECTED_SYSCALL_OR_CALL_IN_PURE_PRIMITIVE'; exit 21
    fi
    python3 "$root/scripts/verify_pure_elf_object.py" "$out/$abi-$backend.o" \
      --abi "$abi" --symbol rafop_rbit32 > "$out/$abi-$backend.json"
    sha256sum "$src" "$out/$abi-$backend.o" "$out/$abi-$backend.s" >> "$out/sha256.txt"
  done
done
# Falsifier: a hosted dependency must not be accepted as a pure object.
printf '%s\n' 'extern unsigned hosted(unsigned); unsigned rafop_rbit32(unsigned x) { return hosted(x); }' > "$out/hosted-negative.c"
"$cc" --target=aarch64-linux-android21 -ffreestanding -c "$out/hosted-negative.c" -o "$out/hosted-negative.o"
if python3 "$root/scripts/verify_pure_elf_object.py" "$out/hosted-negative.o" \
    --abi arm64-v8a --symbol rafop_rbit32 > "$out/hosted-negative.log" 2>&1; then
  echo 'HOSTED_DEPENDENCY_FALSIFIER_SURVIVED'; exit 22
fi
python3 "$root/core/tests/test_pure_elf_object.py" "$out/armeabi-v7a-armv7.o" "$out/arm64-v8a-aarch64.o"
printf '%s\n' 'ARM32_ARM64_FOUR_OBJECTS_PASS_SCOPED; DEVICE_RUNTIME=TOKEN_VAZIO_NOT_RUN'
