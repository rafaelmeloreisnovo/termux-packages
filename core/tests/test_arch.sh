#!/usr/bin/env bash
# REAL: verify architecture matrix and auto-adaptation.
# Status: REAL — runs real binary against real uname output.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

BIN="core/arch-detect"

echo "=== REAL: architecture matrix verification ==="

[ -x "$BIN" ] || { echo "ERROR: $BIN missing"; exit 1; }

# 1) 15 archs enumerated
if $BIN --json 2>/dev/null | grep -q '"total_archs": 15'; then
    pass "15 architectures declared"
else
    fail "wrong total_archs count"
fi

# 2) All 15 canonical names present
for a in x86_64 i386 arm64 arm32 riscv64 riscv32 mips64 mips32 \
         ppc64le ppc32 s390x sparc64 loongarch64 wasm32 arm64_darwin; do
    if $BIN --json 2>/dev/null | grep -q "\"name\":\"$a\""; then
        pass "arch $a present"
    else
        fail "arch $a missing"
    fi
done

# 3) Runtime detection matches uname -m
UN=$(uname -m)
detected=$($BIN --json 2>/dev/null | grep '"runtime_arch"' | \
           sed 's/.*"runtime_arch": *"\([^"]*\)".*/\1/')
# On x86_64 host, either "x86_64" (Linux) or "amd64" would map
case "$UN" in
    x86_64|amd64)   want=x86_64 ;;
    aarch64)        want=arm64 ;;
    armv7*)         want=arm32 ;;
    i686|i386)      want=i386 ;;
    riscv64)        want=riscv64 ;;
    *)              want=unknown ;;
esac
if [ "$detected" = "$want" ]; then
    pass "runtime detect: uname=$UN → $detected (expected $want)"
else
    fail "runtime detect: uname=$UN → $detected (expected $want)"
fi

# 4) Compile-time detection matches (should always be x86_64 on this host)
compile=$($BIN --json 2>/dev/null | grep '"compile_time_arch"' | \
          sed 's/.*"compile_time_arch": *"\([^"]*\)".*/\1/')
if [ "$compile" = "$want" ]; then
    pass "compile-time detect: $compile"
else
    fail "compile-time detect: $compile (expected $want)"
fi

# 5) Compat: x86_64 → i386 is ISA_SUPER + EMULATABLE
out=$($BIN --compat x86_64 i386 2>/dev/null)
if echo "$out" | grep -q "ISA_SUPER  : yes" && \
   echo "$out" | grep -q "EMULATABLE : yes"; then
    pass "compat x86_64→i386: ISA_SUPER + EMULATABLE"
else
    fail "compat x86_64→i386 wrong"
fi

# 6) Compat: arm64 → arm32 is ISA_SUPER + EMULATABLE
out=$($BIN --compat arm64 arm32 2>/dev/null)
if echo "$out" | grep -q "ISA_SUPER  : yes"; then
    pass "compat arm64→arm32: ISA_SUPER"
else
    fail "compat arm64→arm32 wrong"
fi

# 7) Compat: s390x → x86_64 endian mismatch (no ENDIAN flag)
out=$($BIN --compat s390x x86_64 2>/dev/null)
if echo "$out" | grep -q "ENDIAN     : no"; then
    pass "compat s390x→x86_64: endian mismatch detected"
else
    fail "compat s390x→x86_64 endian check wrong"
fi

# 8) Reflexivity: arch → same arch always SAME
for a in x86_64 arm64 riscv64; do
    out=$($BIN --compat $a $a 2>/dev/null)
    if echo "$out" | grep -q "SAME       : yes"; then
        pass "compat $a→$a: SAME"
    else
        fail "compat $a→$a: not SAME"
    fi
done

# 9) big-endian arch table populated correctly
big_count=$($BIN --json 2>/dev/null | grep -c '"endian":"big"')
if [ "$big_count" -ge 4 ]; then
    pass "big-endian archs represented ($big_count found)"
else
    fail "expected >=4 big-endian archs; got $big_count"
fi

# 10) Termux-supported archs marked (4 real: x86_64, i686, aarch64, arm)
termux_count=$($BIN --json 2>/dev/null | grep -cE '"termux":"[^"]+"')
if [ "$termux_count" -eq 4 ]; then
    pass "4 termux-supported archs marked (x86_64, i686, aarch64, arm)"
else
    fail "expected 4 termux archs; got $termux_count"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
