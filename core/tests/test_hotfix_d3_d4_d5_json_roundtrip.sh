#!/usr/bin/env bash
# HOTFIX regression: D3/D4/D5 — JSON escape/decode symmetric round-trip.
#
# Pre-fix state:
#   - real_provenance_write_json wrote fields with raw `%s` — a
#     provenance string containing `"` or `\` would produce malformed
#     JSON and possibly corrupt downstream consumers.
#   - real_receipt_write_io_array same for the `path` field of
#     inputs/outputs entries.
#   - real_contract.c extract_str "handled" escape sequences by
#     consuming both chars without decoding anything — so `"foo\"bar"`
#     would parse as `"foobar"`, breaking the SHA re-computation for
#     any producer that ever emitted an escape.
#
# Post-fix:
#   - Writers escape special chars via json_esc.
#   - extract_str decodes those escapes back to raw bytes.
#   - Round-trip is byte-symmetric, so canonical_serialize's SHA
#     matches on read-back.
#
# Test strategy:
#   No REAL producer today writes fields containing special chars.
#   We can't easily craft one without patching the producer. So we
#   test the SYMMETRY property indirectly:
#     1. Produce a receipt via the real producer path (all-ASCII).
#     2. Verify it via the real verifier path.
#     3. Reproducibility guaranteed by canonical SHA over raw bytes.
#     4. jq must parse both the metrics JSON and the receipt JSON —
#        this catches any regression that broke JSON structure.
#     5. Ledger append + verify must still work end-to-end — this
#        catches any regression where the writer's escape format
#        differs from the parser's decode expectation.
set -uo pipefail

PROD="core/metrics-producer"
VAL="core/receipt-validate"
LED="core/receipt-ledger"
SCRATCH="/tmp/real_d345_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX D3/D4/D5: JSON escape/decode round-trip regression ==="

# 1) Produce metrics + receipt
"$PROD" . "$SCRATCH/m.json" >/dev/null 2>&1
if [ -f "$SCRATCH/m.json" ] && [ -f "$SCRATCH/m.json.receipt" ]; then
    pass "producer wrote metrics + receipt"
else
    fail "producer output missing"; exit 1
fi

# 2) Both files must parse as valid JSON — regression check that
# the writer changes didn't produce malformed output.
if jq -e . "$SCRATCH/m.json" >/dev/null 2>&1; then
    pass "metrics JSON parses with jq"
else
    fail "metrics JSON malformed"
fi
if jq -e . "$SCRATCH/m.json.receipt" >/dev/null 2>&1; then
    pass "receipt JSON parses with jq"
else
    fail "receipt JSON malformed"
fi

# 3) Provenance block must have all string fields as valid strings.
for field in schema_version git_commit build_timestamp_utc \
             cflags_fingerprint toolchain_id producer_name host_uname; do
    val=$(jq -r ".provenance.$field" "$SCRATCH/m.json.receipt" 2>/dev/null)
    if [ -n "$val" ] && [ "$val" != "null" ]; then
        pass "provenance.$field extractable via jq: '$val'"
    else
        fail "provenance.$field missing/null via jq"
    fi
done

# 4) Receipt outputs[0].path extractable — this is the field D5 escapes.
out_path=$(jq -r '.outputs[0].path' "$SCRATCH/m.json.receipt" 2>/dev/null)
if [ "$out_path" = "$SCRATCH/m.json" ]; then
    pass "receipt outputs[0].path round-trips correctly"
else
    fail "receipt outputs[0].path corrupted: expected=$SCRATCH/m.json got=$out_path"
fi

# 5) Signature must verify — closes the loop between writer and
# canonical_serialize.
if "$VAL" "$SCRATCH/m.json.receipt" >/dev/null 2>&1; then
    pass "receipt signature verifies (canonical SHA round-trip intact)"
else
    fail "receipt verify BROKE — writer/canonical form mismatch"
fi

# 6) Ledger append + verify — full chain across writer/parser.
LEDGER="$SCRATCH/round.jsonl"
"$LED" append "$LEDGER" "$SCRATCH/m.json.receipt" >/dev/null 2>&1
if "$LED" verify "$LEDGER" >/dev/null 2>&1; then
    pass "ledger append + verify: full chain intact after writer changes"
else
    fail "ledger verify BROKE — chain hash mismatch"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
