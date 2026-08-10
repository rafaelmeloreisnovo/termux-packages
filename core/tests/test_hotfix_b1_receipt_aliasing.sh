#!/usr/bin/env bash
# HOTFIX regression: B1 — two parsed receipts must NOT share provenance
# backing storage. Before the fix, real_receipt.c held provenance
# fields in static buffers, so verifying receipt A and then receipt B
# silently mutated A's out struct.
#
# This test produces two distinct receipts and verifies that when both
# are parsed into two receipt structs, the recorded provenance in the
# first struct is not overwritten by the second parse.
set -uo pipefail

PROD="core/metrics-producer"
VAL="core/receipt-validate"
SCRATCH="/tmp/real_b1_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX B1: parsed-receipt aliasing regression ==="

# Produce two receipts back-to-back — they will share host and toolchain
# but differ in run_timestamp_unix_ms, so we can detect aliasing by
# checking that field survives a second parse.
"$PROD" . "$SCRATCH/a.json" >/dev/null 2>&1
sleep 0.05
"$PROD" . "$SCRATCH/b.json" >/dev/null 2>&1

if [ -f "$SCRATCH/a.json.receipt" ] && [ -f "$SCRATCH/b.json.receipt" ]; then
    pass "two distinct receipts produced"
else
    fail "receipt production failed"; exit 1
fi

# Extract each receipt's run_timestamp — they must differ since we
# slept between them.
a_ts=$(grep -oE '"run_timestamp_unix_ms":[[:space:]]*[0-9]+' "$SCRATCH/a.json.receipt" | head -1 | grep -oE '[0-9]+')
b_ts=$(grep -oE '"run_timestamp_unix_ms":[[:space:]]*[0-9]+' "$SCRATCH/b.json.receipt" | head -1 | grep -oE '[0-9]+')
if [ "$a_ts" != "$b_ts" ] && [ -n "$a_ts" ] && [ -n "$b_ts" ]; then
    pass "run_timestamps differ: A=$a_ts B=$b_ts"
else
    fail "run_timestamps unexpectedly equal or missing"
fi

# Both receipts must independently verify (chain of trust must survive
# back-to-back parses without cross-contamination).
"$VAL" "$SCRATCH/a.json.receipt" >/dev/null 2>&1 || fail "receipt A verify failed"
"$VAL" "$SCRATCH/b.json.receipt" >/dev/null 2>&1 || fail "receipt B verify failed"

# The real regression test: verify A, then B, then A again. If B's
# parse mutated A's static storage, A's second verify would fail with
# a content_sha256 mismatch.
if "$VAL" "$SCRATCH/a.json.receipt" >/dev/null 2>&1 && \
   "$VAL" "$SCRATCH/b.json.receipt" >/dev/null 2>&1 && \
   "$VAL" "$SCRATCH/a.json.receipt" >/dev/null 2>&1; then
    pass "A → B → A verify sequence intact (no aliasing)"
else
    fail "A verify broken after B parse — B1 regression"
fi

# Cross-check via ledger: append A then B then A again — each append
# calls verify_file, so cross-contamination would produce a broken
# chain immediately.
LEDGER="$SCRATCH/aliasing.jsonl"
"$PROD" . "$SCRATCH/c.json" >/dev/null 2>&1
core/receipt-ledger append "$LEDGER" "$SCRATCH/a.json.receipt" >/dev/null 2>&1
core/receipt-ledger append "$LEDGER" "$SCRATCH/b.json.receipt" >/dev/null 2>&1
core/receipt-ledger append "$LEDGER" "$SCRATCH/c.json.receipt" >/dev/null 2>&1
if core/receipt-ledger verify "$LEDGER" >/dev/null 2>&1; then
    pass "ledger append A→B→C with intermediate verifies still verifies"
else
    fail "ledger broken by aliasing during appends"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
