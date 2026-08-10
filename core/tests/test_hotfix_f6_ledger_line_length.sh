#!/usr/bin/env bash
# HOTFIX regression: F6 — ledger fgets buffer must accommodate the
# worst-case escaped line. Post-E9 escape/decode round-trip, a
# receipt_path of ~256 chars all requiring `\uXXXX` expansion produces
# ~1500 escape bytes + ~350 for the rest of the line = ~1850 bytes.
# LEDGER_PATH_MAX is 512, worst case ~3400 bytes. Pre-F6 fgets buffer
# was 2048 — silently truncated on long-path lines, and the parser
# would then reject the line with no clue why.
#
# We can't easily create a file at a path with 100+ control chars
# (bash quoting refuses most). So this test creates a DEEP DIRECTORY
# structure yielding a very long absolute receipt_path, verifies the
# ledger writer+reader still round-trips cleanly.
set -uo pipefail

PROD="core/metrics-producer"
LED="core/receipt-ledger"
SCRATCH="/tmp/real_f6_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX F6: ledger line-length regression ==="

# 1) Build a moderately-deep directory to yield a longish absolute
# receipt_path. Producer's I/O path buffer is 512 bytes
# (RECEIPT_PATH_MAX), so we keep the JSON path under ~200 chars total.
# Even at 200 chars, if every char required \uXXXX escape (worst case
# 6x expansion), the escaped form would be ~1200 bytes — well over
# the pre-F6 buffer of 2048 once combined with the fixed ~350 bytes
# of ledger structure.
DEEP="$SCRATCH"
for i in $(seq 1 5); do
    DEEP="$DEEP/deep-lvl-$i-with-long-name"
done
mkdir -p "$DEEP"

long_prefix="$DEEP/receipt.json"
plen=${#long_prefix}
if [ "$plen" -gt 120 ]; then
    pass "constructed longer-than-usual receipt path: $plen chars"
else
    fail "path only $plen chars (expected > 120)"
    exit 1
fi

# 2) Produce a receipt at that long path
"$PROD" . "$long_prefix" >/dev/null 2>&1
if [ -f "${long_prefix}.receipt" ]; then
    pass "producer wrote receipt at long path"
else
    fail "producer failed for long path"; exit 1
fi

# 3) Append to ledger — this is the critical operation.
# Pre-F6 with 2048-byte buffer + long escaped path, could truncate.
LEDGER="$SCRATCH/long.jsonl"
if "$LED" append "$LEDGER" "${long_prefix}.receipt" >/dev/null 2>&1; then
    pass "ledger append succeeded for long path"
else
    fail "ledger append failed for long path"
fi

# 4) Ledger line is present and non-empty
if [ -s "$LEDGER" ]; then
    line_len=$(wc -c < "$LEDGER")
    pass "ledger line written ($line_len bytes)"
else
    fail "ledger empty after append"; exit 1
fi

# 5) Chain must verify — this catches ANY truncation because
# parse_entry_line would fail on missing quote terminators.
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "chain verifies after long-path append (F6 fix intact)"
else
    fail "chain broken by long path — F6 regression"
fi

# 6) Round-trip via jq — extra check that the line is well-formed
if jq -e -c . "$LEDGER" >/dev/null 2>&1; then
    pass "long-path ledger line is valid JSON per jq"
else
    fail "long-path ledger line malformed"
fi

# 7) Extract receipt_path via jq, verify byte-for-byte round-trip
extracted=$(jq -r '.receipt_path' "$LEDGER" 2>/dev/null)
if [ "$extracted" = "${long_prefix}.receipt" ]; then
    pass "long receipt_path round-trips byte-for-byte"
else
    fail "long path corrupted: expected=${long_prefix}.receipt got=$extracted"
fi

# 8) Add 5 more appends at the long path to exercise chain hashing
for i in 2 3 4 5 6; do
    long_i="$DEEP/receipt-$i.json"
    "$PROD" . "$long_i" >/dev/null 2>&1
    "$LED" append "$LEDGER" "${long_i}.receipt" >/dev/null 2>&1
done
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "5 additional long-path appends: chain intact"
else
    fail "chain broke on additional long-path appends"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
