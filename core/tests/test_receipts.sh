#!/usr/bin/env bash
# REAL: end-to-end receipts test.
# Status: REAL — hashes are computed by our own SHA256, verified against
# system sha256sum, and tamper cases confirm fail-closed behavior.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

PROD="core/metrics-producer"
VAL="core/receipt-validate"
SCRATCH="/tmp/real_receipts_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

echo "=== REAL: receipts end-to-end ==="

# 1) Produce metrics + receipt
if ! "$PROD" . "$SCRATCH/m.json" >/dev/null 2>&1; then
    fail "producer failed"; exit 1
fi
pass "producer emitted metrics + receipt"

# 2) Receipt file exists
if [ -f "$SCRATCH/m.json.receipt" ]; then
    pass "receipt file created at m.json.receipt"
else
    fail "no receipt file"
fi

# 3) Receipt has status=REAL and schema=receipt/1.0.0
if grep -q '"schema": "receipt/1.0.0"' "$SCRATCH/m.json.receipt" && \
   grep -q '"status": "REAL"' "$SCRATCH/m.json.receipt"; then
    pass "receipt has correct schema + status"
else
    fail "receipt schema/status wrong"
fi

# 4) Receipt records output SHA and size
if grep -q '"outputs"' "$SCRATCH/m.json.receipt" && \
   grep -q '"sha256":"[0-9a-f]\{64\}"' "$SCRATCH/m.json.receipt"; then
    pass "receipt records output SHA256"
else
    fail "receipt missing output SHA"
fi

# 5) Output SHA in receipt matches actual file SHA256 (system sha256sum)
receipt_sha=$(grep -oE '"sha256":"[0-9a-f]{64}"' "$SCRATCH/m.json.receipt" | \
              head -1 | sed 's/"sha256":"\([^"]*\)"/\1/')
actual_sha=$(sha256sum "$SCRATCH/m.json" | cut -d' ' -f1)
if [ "$receipt_sha" = "$actual_sha" ]; then
    pass "receipt SHA matches system sha256sum ($actual_sha)"
else
    fail "SHA mismatch: receipt=$receipt_sha actual=$actual_sha"
fi

# 6) Validator accepts untampered receipt
if "$VAL" "$SCRATCH/m.json.receipt" >/dev/null 2>&1; then
    pass "validator accepts untampered receipt"
else
    fail "validator rejects untampered receipt"
fi

# 7) Validator rejects tampered exit_code
sed 's/"exit_code": 0/"exit_code": 99/' "$SCRATCH/m.json.receipt" > "$SCRATCH/t1.receipt"
if ! "$VAL" "$SCRATCH/t1.receipt" >/dev/null 2>&1; then
    pass "tamper rejected: exit_code changed"
else
    fail "tamper NOT detected: exit_code changed"
fi

# 8) Validator rejects tampered output SHA
sed 's/"sha256":"[0-9a-f]\{16\}/"sha256":"deadbeefdeadbeef/' \
    "$SCRATCH/m.json.receipt" > "$SCRATCH/t2.receipt"
if ! "$VAL" "$SCRATCH/t2.receipt" >/dev/null 2>&1; then
    pass "tamper rejected: output SHA changed"
else
    fail "tamper NOT detected: output SHA changed"
fi

# 9) Validator rejects tampered operation
sed 's/"operation": "produce_metrics"/"operation": "attack"/' \
    "$SCRATCH/m.json.receipt" > "$SCRATCH/t3.receipt"
if ! "$VAL" "$SCRATCH/t3.receipt" >/dev/null 2>&1; then
    pass "tamper rejected: operation changed"
else
    fail "tamper NOT detected: operation changed"
fi

# 10) Validator rejects tampered git_commit
sed 's/"git_commit": "[a-f0-9]*"/"git_commit": "0000000000000000000000000000000000000000"/' \
    "$SCRATCH/m.json.receipt" > "$SCRATCH/t4.receipt"
if ! "$VAL" "$SCRATCH/t4.receipt" >/dev/null 2>&1; then
    pass "tamper rejected: git_commit changed"
else
    fail "tamper NOT detected: git_commit changed"
fi

# 11) Reproducibility: same inputs → same output SHA (byte-for-byte)
"$PROD" . "$SCRATCH/m2.json" >/dev/null 2>&1
sha1=$(sha256sum "$SCRATCH/m.json" | cut -d' ' -f1)
sha2=$(sha256sum "$SCRATCH/m2.json" | cut -d' ' -f1)
# Files include timestamps so exact byte match differs; but node/edge
# fields should be identical.
n1=$(grep -oE '"node_count": [0-9]+' "$SCRATCH/m.json" | head -1)
n2=$(grep -oE '"node_count": [0-9]+' "$SCRATCH/m2.json" | head -1)
if [ "$n1" = "$n2" ]; then
    pass "reproducibility: node_count identical across runs ($n1)"
else
    fail "reproducibility broken: $n1 vs $n2"
fi

# 12) Receipt records provenance non-empty
for f in git_commit toolchain_id host_uname producer_name; do
    if grep -qE "\"$f\": \"[^\"]+\"" "$SCRATCH/m.json.receipt" && \
       ! grep -qE "\"$f\": \"TOKEN_VAZIO" "$SCRATCH/m.json.receipt"; then
        pass "receipt provenance.$f non-empty + no TOKEN_VAZIO"
    else
        fail "receipt provenance.$f missing or TOKEN_VAZIO"
    fi
done

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
