#!/usr/bin/env bash
# REAL: end-to-end chain-of-custody ledger test.
# Status: REAL — every hash is computed, every tamper is real.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

PROD="core/metrics-producer"
LED="core/receipt-ledger"
SCRATCH="/tmp/real_ledger_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

LEDGER="$SCRATCH/custody.jsonl"

echo "=== REAL: chain-of-custody ledger end-to-end ==="

# 1) Produce 3 receipts
for i in 1 2 3; do
    "$PROD" . "$SCRATCH/r$i.json" >/dev/null 2>&1
done
if [ -f "$SCRATCH/r1.json.receipt" ] && \
   [ -f "$SCRATCH/r2.json.receipt" ] && \
   [ -f "$SCRATCH/r3.json.receipt" ]; then
    pass "3 receipts produced"
else
    fail "receipt generation failed"; exit 1
fi

# 2) Append all 3
for i in 1 2 3; do
    if "$LED" append "$LEDGER" "$SCRATCH/r$i.json.receipt" >/dev/null 2>&1; then
        pass "append receipt $i"
    else
        fail "append receipt $i failed"
    fi
done

# 3) Verify intact
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "verify intact chain PASS"
else
    fail "verify intact chain FAILED"
fi

# 4) tail reports 3 entries
tail_out=$("$LED" tail "$LEDGER" 2>/dev/null)
if echo "$tail_out" | grep -q "entry_count:   3"; then
    pass "tail reports 3 entries"
else
    fail "tail count wrong"
fi

# 5) chain-hash: entry 1 prev_tail_sha256 == entry 0 entry_sha256
e0_hash=$(sed -n '1p' "$LEDGER" | \
          grep -oE '"entry_sha256":"[a-f0-9]{64}"' | \
          sed 's/"entry_sha256":"\([^"]*\)"/\1/')
e1_prev=$(sed -n '2p' "$LEDGER" | \
          grep -oE '"prev_tail_sha256":"[a-f0-9]{64}"' | \
          sed 's/"prev_tail_sha256":"\([^"]*\)"/\1/')
if [ -n "$e0_hash" ] && [ "$e0_hash" = "$e1_prev" ]; then
    pass "chain-hash linked: entry1.prev = entry0.self ($e0_hash)"
else
    fail "chain-hash NOT linked: e0=$e0_hash e1_prev=$e1_prev"
fi

# 6) tamper mid-chain receipt_sha256 → chain breaks (target line 2 specifically)
sed '2s/"receipt_sha256":"[a-f0-9]\{16\}/"receipt_sha256":"deadbeefdeadbeef/' \
    "$LEDGER" > "$SCRATCH/t_mid.jsonl"
if ! "$LED" verify "$SCRATCH/t_mid.jsonl" >/dev/null 2>&1; then
    pass "tamper detected: mid-chain receipt_sha256 changed"
else
    fail "tamper NOT detected: mid-chain receipt_sha256"
fi

# 7) tamper referenced receipt file → chain breaks
cp "$SCRATCH/r2.json.receipt" "$SCRATCH/r2.json.receipt.bak"
sed 's/"exit_code": 0/"exit_code": 42/' \
    "$SCRATCH/r2.json.receipt.bak" > "$SCRATCH/r2.json.receipt"
if ! "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "tamper detected: referenced receipt file altered"
else
    fail "tamper NOT detected: receipt file altered"
fi
mv "$SCRATCH/r2.json.receipt.bak" "$SCRATCH/r2.json.receipt"

# 8) delete a middle entry → seq gap + break
sed '2d' "$LEDGER" > "$SCRATCH/t_del.jsonl"
if ! "$LED" verify "$SCRATCH/t_del.jsonl" >/dev/null 2>&1; then
    pass "tamper detected: middle entry deleted (seq gap)"
else
    fail "tamper NOT detected: middle entry deleted"
fi

# 9) reorder entries → chain breaks at first
tac "$LEDGER" > "$SCRATCH/t_reorder.jsonl"
if ! "$LED" verify "$SCRATCH/t_reorder.jsonl" >/dev/null 2>&1; then
    pass "tamper detected: entries reordered"
else
    fail "tamper NOT detected: entries reordered"
fi

# 10) remove a referenced receipt file → chain breaks
mv "$SCRATCH/r3.json.receipt" "$SCRATCH/r3.json.receipt.hidden"
if ! "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "tamper detected: referenced receipt file missing"
else
    fail "tamper NOT detected: receipt missing"
fi
mv "$SCRATCH/r3.json.receipt.hidden" "$SCRATCH/r3.json.receipt"

# 11) after restoring, chain verifies again (idempotent)
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "chain verifies again after restore (idempotent)"
else
    fail "chain broken after restore"
fi

# 12) append one more → chain still intact
"$PROD" . "$SCRATCH/r4.json" >/dev/null 2>&1
"$LED" append "$LEDGER" "$SCRATCH/r4.json.receipt" >/dev/null 2>&1
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "append after verify: chain still intact (4 entries)"
else
    fail "append broke chain"
fi

# 13) sequential seq numbering (0..3)
seqs=$(grep -oE '"seq":[0-9]+' "$LEDGER" | grep -oE '[0-9]+')
if [ "$seqs" = "$(printf '0\n1\n2\n3')" ]; then
    pass "seq numbers strictly sequential 0..3"
else
    fail "seq numbers not sequential: $seqs"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
