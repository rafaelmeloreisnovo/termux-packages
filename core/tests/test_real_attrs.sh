#!/usr/bin/env bash
# REAL: Verify mini-block attributes reach the linker.
# Status: REAL — inspects actual object files with readelf.

set -uo pipefail

PASSED=0
FAILED=0

pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== REAL: attribute-directive verification ==="

# Build a non-LTO .o to inspect per-function sections
SCRATCH="/tmp/real_attrs_test"
mkdir -p "$SCRATCH"
cc -Wall -Wextra -Wshadow -Werror -O2 -g \
   -fdata-sections -ffunction-sections -fvisibility=hidden \
   -c -o "$SCRATCH/pkg_scanner.o" core/pkg_scanner.c 2>/dev/null

if [ ! -f "$SCRATCH/pkg_scanner.o" ]; then
    echo "ERROR: could not build pkg_scanner.o"
    exit 1
fi

# 1) Hot functions get .text.hot sections
if readelf -SW "$SCRATCH/pkg_scanner.o" 2>/dev/null | grep -q '\.text\.hot\.'; then
    pass "REAL_HOT → per-function .text.hot sections"
else
    fail "REAL_HOT did not produce .text.hot sections"
fi

# 2) Cold functions get .text.unlikely (or .text.cold) sections
if readelf -SW "$SCRATCH/pkg_scanner.o" 2>/dev/null | \
   grep -qE '\.text\.(cold|unlikely)\.'; then
    pass "REAL_COLD → per-function .text.cold/.unlikely sections"
else
    fail "REAL_COLD did not produce .text.cold/.unlikely sections"
fi

# 3) Count total per-function sections — should be many
per_fn_sections=$(readelf -SW "$SCRATCH/pkg_scanner.o" 2>/dev/null | \
                  grep -c '\.text\..*PROGBITS')
if [ "$per_fn_sections" -ge 10 ]; then
    pass "$per_fn_sections per-function .text.* sections (gc-able)"
else
    fail "only $per_fn_sections per-function sections (expected >=10)"
fi

# 4) Executable: visibility hidden — no exports from our REAL code
BIN="core/metrics-producer"
if [ -x "$BIN" ]; then
    # Only symbols the runtime demands should be GLOBAL DEFAULT
    exposed=$(readelf --dyn-syms -W "$BIN" 2>/dev/null | \
              awk '$5=="GLOBAL" && $7=="DEFAULT" && $6=="FUNC" {c++} END {print c+0}')
    if [ "$exposed" -le 10 ]; then
        pass "hidden visibility: only $exposed exported functions"
    else
        fail "too many exported functions: $exposed"
    fi

    # 5) Producer runs and emits status=REAL
    if $BIN . "$SCRATCH/smoke.json" >/dev/null 2>&1; then
        if grep -q '"status": "REAL"' "$SCRATCH/smoke.json"; then
            pass "producer emits status=REAL end-to-end"
        else
            fail "output missing status=REAL"
        fi
    else
        fail "producer run failed"
    fi
else
    fail "metrics-producer binary missing (make first)"
fi

rm -rf "$SCRATCH"

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
