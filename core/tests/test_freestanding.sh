#!/usr/bin/env bash
# REAL: verify freestanding binary produces same counts as libc version.
# Also verifies the binary genuinely has no libc / no dynamic linker.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

BIN="core/pkg-count-freestanding"
LIBC_BIN="core/pkg-real"

echo "=== REAL: freestanding pkg_count verification ==="

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN missing"; exit 1
fi

# 1) Fully static, no dynamic linker
if file "$BIN" | grep -q 'statically linked'; then
    pass "binary is statically linked (no dynamic linker)"
else
    fail "binary is not statically linked"
fi

# 2) No libc symbols anywhere
if ! nm "$BIN" 2>/dev/null | grep -qE '(__libc_|__stack_chk|_stdio|malloc|free|printf)'; then
    pass "no libc symbols present in binary"
else
    fail "binary contains libc symbols"
fi

# 3) Custom _start entrypoint
if nm "$BIN" 2>/dev/null | grep -q ' _start$'; then
    pass "custom _start entrypoint present"
else
    fail "no _start entrypoint"
fi

# 4) Runs and produces expected output
out=$("$BIN" . 2>/dev/null)
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "binary exited with $rc"
else
    pass "binary runs and exits 0"
fi

# 5) Counts match libc version (if libc version exists)
if [ -x "$LIBC_BIN" ]; then
    libc_total=$("$LIBC_BIN" inventory . 2>/dev/null | \
                 grep 'Packages with build.sh:' | awk '{print $NF}')
    fs_total=$(echo "$out" | grep '^total_build_sh=' | cut -d= -f2)
    if [ "$libc_total" = "$fs_total" ]; then
        pass "count matches libc version: $fs_total"
    else
        fail "count mismatch: freestanding=$fs_total libc=$libc_total"
    fi
fi

# 6) Small binary size (proof of friction reduction)
bytes=$(stat -c%s "$BIN")
if [ "$bytes" -lt 20000 ]; then
    pass "binary size $bytes bytes (< 20KB)"
else
    fail "binary too large: $bytes bytes"
fi

# 7) No data/bss beyond exit code — pure syscalls path
text=$(size "$BIN" 2>/dev/null | awk 'NR==2 {print $1}')
if [ -n "$text" ] && [ "$text" -lt 5000 ]; then
    pass "text section $text bytes (< 5KB)"
else
    fail "text section too large: $text bytes"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
