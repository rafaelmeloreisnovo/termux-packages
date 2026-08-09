#!/usr/bin/env bash
# REAL: verify runtime capability probe (Phase 10).
# Status: OBSERVED — reads /proc/cpuinfo, sysconf, /sys, uname.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

PROBE="core/arch-probe"
VAL="core/receipt-validate"
LED="core/receipt-ledger"
SCRATCH="/tmp/real_probe_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

echo "=== REAL: runtime capability probe (Phase 10) ==="

# 1) Probe runs
if "$PROBE" "$SCRATCH/cap.json" >/dev/null 2>&1; then
    pass "arch-probe runs"
else
    fail "arch-probe failed"; exit 1
fi

# 2) Emits arch_capability/1.0.0
if grep -q '"schema": "arch_capability/1.0.0"' "$SCRATCH/cap.json"; then
    pass "schema=arch_capability/1.0.0"
else
    fail "wrong schema"
fi

# 3) Status is OBSERVED (not REAL — different scope)
if grep -q '"status": "OBSERVED"' "$SCRATCH/cap.json"; then
    pass "status=OBSERVED"
else
    fail "status wrong"
fi

# 4) Observed page_size is > 0
obs_page=$(grep -oE '"page_size": *[0-9]+' "$SCRATCH/cap.json" | \
           head -1 | grep -oE '[0-9]+')
if [ "$obs_page" -ge 4096 ] && [ "$obs_page" -le 65536 ]; then
    pass "observed page_size = $obs_page (in realistic range)"
else
    fail "observed page_size $obs_page unrealistic"
fi

# 5) Observed page_size matches sysconf PAGESIZE
sys_page=$(getconf PAGESIZE)
if [ "$obs_page" = "$sys_page" ]; then
    pass "observed page_size matches getconf PAGESIZE ($sys_page)"
else
    fail "observed=$obs_page vs getconf=$sys_page"
fi

# 6) Observed cache_line > 0 on most systems
obs_cache=$(grep -oE '"cache_line": *[0-9]+' "$SCRATCH/cap.json" | \
            head -1 | grep -oE '[0-9]+')
if [ "$obs_cache" -gt 0 ]; then
    pass "observed cache_line = $obs_cache bytes"
else
    fail "observed cache_line missing/zero"
fi

# 7) identity.uname_machine matches uname -m
u=$(uname -m)
if grep -q "\"uname_machine\": \"$u\"" "$SCRATCH/cap.json"; then
    pass "identity.uname_machine = $u"
else
    fail "uname_machine mismatch"
fi

# 8) authority declared observed_supersedes_nominal
if grep -q '"authority": *"observed_supersedes_nominal"' "$SCRATCH/cap.json"; then
    pass "authority: observed_supersedes_nominal"
else
    fail "authority field wrong"
fi

# 9) receipt sealed
if [ -f "$SCRATCH/cap.json.receipt" ]; then
    pass "receipt file created"
else
    fail "no receipt file"
fi

# 10) receipt signature verifies
if "$VAL" "$SCRATCH/cap.json.receipt" >/dev/null 2>&1; then
    pass "receipt signature valid"
else
    fail "receipt signature invalid"
fi

# 11) receipt records arch_probe as operation
if grep -q '"operation": "arch_probe"' "$SCRATCH/cap.json.receipt"; then
    pass "receipt operation=arch_probe"
else
    fail "receipt operation wrong"
fi

# 12) receipt SHA of output matches actual file SHA
receipt_sha=$(grep -oE '"sha256":"[a-f0-9]{64}"' "$SCRATCH/cap.json.receipt" | \
              head -1 | sed 's/"sha256":"\([^"]*\)"/\1/')
actual_sha=$(sha256sum "$SCRATCH/cap.json" | cut -d' ' -f1)
if [ "$receipt_sha" = "$actual_sha" ]; then
    pass "receipt SHA matches actual sha256sum"
else
    fail "SHA mismatch"
fi

# 13) capability receipt can be appended to ledger
LEDGER="$SCRATCH/probe_chain.jsonl"
if "$LED" append "$LEDGER" "$SCRATCH/cap.json.receipt" >/dev/null 2>&1; then
    pass "capability receipt appends to ledger"
else
    fail "ledger append failed"
fi

# 14) ledger verify passes on capability-only chain
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "capability ledger verifies"
else
    fail "capability ledger verify failed"
fi

# 15) reproducibility: probe again → same observed values
"$PROBE" "$SCRATCH/cap2.json" >/dev/null 2>&1
p2=$(grep -oE '"page_size": *[0-9]+' "$SCRATCH/cap2.json" | \
     head -1 | grep -oE '[0-9]+')
c2=$(grep -oE '"cache_line": *[0-9]+' "$SCRATCH/cap2.json" | \
     head -1 | grep -oE '[0-9]+')
if [ "$obs_page" = "$p2" ] && [ "$obs_cache" = "$c2" ]; then
    pass "reproducibility: page=$p2 cache=$c2 identical across runs"
else
    fail "not reproducible: page $obs_page→$p2 cache $obs_cache→$c2"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
