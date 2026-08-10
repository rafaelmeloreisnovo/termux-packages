#!/usr/bin/env bash
# HOTFIX regression: B3/B4 — tampered numeric fields in receipts and
# ledger entries must be rejected AT PARSE TIME, not silently coerced
# to 0. Before the fix, `strtoull("garbage", NULL, 10) == 0` was
# accepted; detection relied entirely on downstream hash re-checks.
set -uo pipefail

PROD="core/metrics-producer"
VAL="core/receipt-validate"
LED="core/receipt-ledger"
SCRATCH="/tmp/real_b3b4_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX B3/B4: strto* parse-time rejection regression ==="

"$PROD" . "$SCRATCH/r.json" >/dev/null 2>&1
"$VAL" "$SCRATCH/r.json.receipt" >/dev/null 2>&1 || { fail "clean receipt should verify"; exit 1; }
pass "clean receipt verifies"

# Tamper: replace a numeric field with garbage (not just wrong digits,
# but non-parseable text). Parser must fail-closed at parse.
sed 's/"duration_us": [0-9]*/"duration_us": abcxyz/' \
    "$SCRATCH/r.json.receipt" > "$SCRATCH/tampered_dur.receipt"
if "$VAL" "$SCRATCH/tampered_dur.receipt" >/dev/null 2>&1; then
    fail "tampered duration_us=abcxyz was accepted (B4 regression)"
else
    pass "tampered duration_us rejected at parse time"
fi

sed 's/"started_unix_ms": [0-9]*/"started_unix_ms": zzz/' \
    "$SCRATCH/r.json.receipt" > "$SCRATCH/tampered_start.receipt"
if "$VAL" "$SCRATCH/tampered_start.receipt" >/dev/null 2>&1; then
    fail "tampered started_unix_ms=zzz was accepted"
else
    pass "tampered started_unix_ms rejected"
fi

# Ledger tamper: garbage seq number
LEDGER="$SCRATCH/l.jsonl"
"$LED" append "$LEDGER" "$SCRATCH/r.json.receipt" >/dev/null 2>&1

sed 's/"seq":[0-9]*/"seq":XYZ/' "$LEDGER" > "$SCRATCH/tampered_seq.jsonl"
if "$LED" verify "$SCRATCH/tampered_seq.jsonl" >/dev/null 2>&1; then
    fail "ledger with garbage seq accepted (B3 regression)"
else
    pass "ledger garbage seq rejected at parse"
fi

# Ledger tamper: garbage appended_unix_ms
sed 's/"appended_unix_ms":[0-9]*/"appended_unix_ms":junk/' \
    "$LEDGER" > "$SCRATCH/tampered_ms.jsonl"
if "$LED" verify "$SCRATCH/tampered_ms.jsonl" >/dev/null 2>&1; then
    fail "ledger garbage appended_unix_ms accepted"
else
    pass "ledger garbage appended_unix_ms rejected"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
