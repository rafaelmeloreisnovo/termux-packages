#!/usr/bin/env bash
# Architecture identity + nominal reference matrix verification.
# Status: OBSERVED_LIMITED — host identity is observed; the 15-entry table is
# reference metadata, not 15-architecture execution evidence.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

BIN="core/arch-detect"

echo "=== architecture identity + NOMINAL matrix verification ==="

[ -x "$BIN" ] || { echo "ERROR: $BIN missing"; exit 1; }

JSON=$($BIN --json 2>/dev/null)

# 1) Scope must not claim the nominal table as global runtime truth.
if echo "$JSON" | grep -q '"status": "OBSERVED_LIMITED"' && \
   echo "$JSON" | grep -q '"claim_allowed": false' && \
   echo "$JSON" | grep -q '"property_scope": "nominal_reference_not_runtime_capability"'; then
    pass "matrix scope is OBSERVED_LIMITED + claim_allowed=false"
else
    fail "matrix scope overclaims runtime capability"
fi

# 2) 15 identifiers enumerated.
if echo "$JSON" | grep -q '"total_archs": 15'; then
    pass "15 architecture identifiers declared"
else
    fail "wrong total_archs count"
fi

# 3) All 15 canonical names present.
for a in x86_64 i386 arm64 arm32 riscv64 riscv32 mips64 mips32 \
         ppc64le ppc32 s390x sparc64 loongarch64 wasm32 arm64_darwin; do
    if echo "$JSON" | grep -q "\"name\":\"$a\""; then
        pass "arch id $a present"
    else
        fail "arch id $a missing"
    fi
done

# 4) Runtime identity matches uname -m for mappings this test knows.
UN=$(uname -m)
detected=$(echo "$JSON" | grep '"runtime_arch"' | \
           sed 's/.*"runtime_arch": *"\([^"]*\)".*/\1/')
case "$UN" in
    x86_64|amd64)   want=x86_64 ;;
    aarch64)        want=arm64 ;;
    arm64)          want=arm64 ;;
    armv7*)         want=arm32 ;;
    i686|i386)      want=i386 ;;
    riscv64)        want=riscv64 ;;
    *)              want=unknown ;;
esac
if [ "$detected" = "$want" ]; then
    pass "runtime identity: uname=$UN → $detected"
else
    fail "runtime identity: uname=$UN → $detected (expected $want)"
fi

# 5) Compile-time identity matches the native compiler host in this CI route.
compile=$(echo "$JSON" | grep '"compile_time_arch"' | \
          sed 's/.*"compile_time_arch": *"\([^"]*\)".*/\1/')
if [ "$compile" = "$want" ]; then
    pass "compile-time identity: $compile"
else
    fail "compile-time identity: $compile (expected $want)"
fi

# 6) x86_64 -> i386 may retain a nominal ISA-family relationship, but static
# catalog data must NOT claim emulator availability.
out=$($BIN --compat x86_64 i386 2>/dev/null)
if echo "$out" | grep -q "ISA_SUPER  : yes" && \
   echo "$out" | grep -q "EMULATABLE : no"; then
    pass "x86_64→i386 nominal ISA relation without emulation claim"
else
    fail "x86_64→i386 relation incorrectly claims runtime emulation"
fi

# 7) ARM64 -> ARM32 execution support is implementation/OS-dependent and must
# not be promoted from the static table.
out=$($BIN --compat arm64 arm32 2>/dev/null)
if echo "$out" | grep -q "ISA_SUPER  : no" && \
   echo "$out" | grep -q "EMULATABLE : no"; then
    pass "arm64→arm32 runtime support left unclaimed"
else
    fail "arm64→arm32 static table overclaims execution support"
fi

# 8) No arbitrary cross-arch pair may be statically marked emulatable.
for pair in "x86_64 arm64" "arm64 riscv64" "riscv64 riscv32" "mips64 mips32"; do
    set -- $pair
    out=$($BIN --compat "$1" "$2" 2>/dev/null)
    if echo "$out" | grep -q "EMULATABLE : no"; then
        pass "compat $1→$2: no static emulator claim"
    else
        fail "compat $1→$2: static emulator claim leaked"
    fi
done

# 9) Reflexivity remains a catalog invariant.
for a in x86_64 arm64 riscv64; do
    out=$($BIN --compat "$a" "$a" 2>/dev/null)
    if echo "$out" | grep -q "SAME       : yes"; then
        pass "compat $a→$a: SAME"
    else
        fail "compat $a→$a: not SAME"
    fi
done

# 10) Endianness catalog remains populated.
big_count=$(echo "$JSON" | grep -c '"endian":"big"')
if [ "$big_count" -ge 4 ]; then
    pass "big-endian reference profiles represented ($big_count found)"
else
    fail "expected >=4 big-endian profiles; got $big_count"
fi

# 11) Termux-supported identifiers remain exactly the historical four.
termux_count=$(echo "$JSON" | grep -cE '"termux":"[^"]+"')
if [ "$termux_count" -eq 4 ]; then
    pass "4 Termux architecture identifiers marked"
else
    fail "expected 4 Termux arch ids; got $termux_count"
fi

# 12) Capability-like fields must be named nominal_* in emitted JSON.
if echo "$JSON" | grep -q '"nominal_page_size"' && \
   echo "$JSON" | grep -q '"nominal_cache_line"' && \
   echo "$JSON" | grep -q '"nominal_simd"'; then
    pass "capability-like fields are explicitly nominal"
else
    fail "nominal capability field names missing"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
