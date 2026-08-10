#!/usr/bin/env bash
# HOTFIX regression: B6 — concurrent real_ledger_append must serialise
# via flock() and produce a well-formed chain (or fail-closed on one
# of the racers), never a silently broken chain that verify catches
# only after the fact.
#
# Before the fix, two concurrent `receipt-ledger append` calls on the
# same ledger both read next_seq=N, both wrote seq=N + prev_tail=T,
# and one entry silently overwrote/duplicated seq. After the fix
# flock(LOCK_EX) serialises the tail-read+write critical section.
set -uo pipefail

PROD="core/metrics-producer"
LED="core/receipt-ledger"
SCRATCH="/tmp/real_b6_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX B6: concurrent ledger-append race regression ==="

# Produce 10 distinct receipts
for i in $(seq 1 10); do
    "$PROD" . "$SCRATCH/r$i.json" >/dev/null 2>&1
done
if [ -f "$SCRATCH/r10.json.receipt" ]; then
    pass "10 receipts produced"
else
    fail "receipt production failed"; exit 1
fi

LEDGER="$SCRATCH/concurrent.jsonl"

# Fire 10 appenders simultaneously against the SAME ledger. flock()
# must serialise them; the final chain must be intact and contain
# exactly 10 sequential entries (0..9).
for i in $(seq 1 10); do
    "$LED" append "$LEDGER" "$SCRATCH/r$i.json.receipt" >/dev/null 2>&1 &
done
wait

# Chain must verify.
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "concurrent chain intact — flock serialised writers"
else
    fail "concurrent chain BROKEN — B6 regression"
fi

# Entries: exactly 10, seq 0..9 no gaps.
n=$(wc -l < "$LEDGER")
if [ "$n" -eq 10 ]; then
    pass "exactly 10 entries written"
else
    fail "wrong entry count: $n (expected 10)"
fi

seqs=$(grep -oE '"seq":[0-9]+' "$LEDGER" | grep -oE '[0-9]+' | sort -n | tr '\n' ' ')
expected="0 1 2 3 4 5 6 7 8 9 "
if [ "$seqs" = "$expected" ]; then
    pass "seq 0..9 present, no duplicates or gaps"
else
    fail "seq malformed: [$seqs]"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
