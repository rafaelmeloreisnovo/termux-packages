#!/usr/bin/env bash
# HOTFIX regression: G6 — metrics-producer and arch-probe must return
# non-zero exit when the receipt companion fails, not silently return
# 0 after writing only the JSON.
#
# Pre-fix: if real_receipt_write() failed (e.g., disk full while
# writing the receipt file), producer wrote WARN to stderr but
# returned 0. Callers parsing exit code saw "success" — but the audit
# trail was silently incomplete. Governance would catch it at step 8
# (receipt-not-emitted) but the producer's own contract was broken.
#
# Post-fix: exit 3 = "partial success — JSON OK, receipt failed".
#
# Negative-path injection is REAL and production-neutral: the receipt
# writer creates <receipt>.tmp.<pid> with O_CREAT|O_EXCL|O_NOFOLLOW.
# A short bash wrapper pre-occupies that exact temporary path and then
# exec()s the producer. exec preserves the PID, so JSON creation can
# succeed while real_receipt_write() deterministically hits EEXIST.
# No production code, environment backdoor, LD_PRELOAD, or mock
# filesystem is required.
set -uo pipefail

PROD="core/metrics-producer"
PROBE="core/arch-probe"
SCRATCH="/tmp/real_g6_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX G6: producer exit code honesty regression ==="

# 1) Happy-path metrics-producer: exit 0 AND receipt present.
if "$PROD" . "$SCRATCH/m.json" >/dev/null 2>&1; then
    if [ -f "$SCRATCH/m.json" ] && [ -f "$SCRATCH/m.json.receipt" ]; then
        pass "metrics-producer happy path: exit 0 + JSON + receipt all present"
    else
        fail "metrics-producer exit 0 but files missing"
    fi
else
    fail "metrics-producer happy path failed unexpectedly"
fi

# 2) Happy-path arch-probe: same invariant.
if "$PROBE" "$SCRATCH/c.json" >/dev/null 2>&1; then
    if [ -f "$SCRATCH/c.json" ] && [ -f "$SCRATCH/c.json.receipt" ]; then
        pass "arch-probe happy path: exit 0 + JSON + receipt all present"
    else
        fail "arch-probe exit 0 but files missing"
    fi
else
    fail "arch-probe happy path failed unexpectedly"
fi

# 3-5) TRUE negative path for metrics-producer.
# The wrapper's PID becomes the producer PID after exec. The sentinel
# therefore occupies exactly m-neg.json.receipt.tmp.<producer-pid>.
MNEG="$SCRATCH/m-neg.json"
MNEG_ERR="$SCRATCH/m-neg.err"
OUT="$MNEG" BIN="$PROD" bash -c '
    tmp="${OUT}.receipt.tmp.$$"
    printf "%s\n" "G6_PREEXISTING_SENTINEL" > "$tmp"
    exec "$BIN" . "$OUT"
' >/dev/null 2>"$MNEG_ERR"
mrc=$?

if [ "$mrc" -eq 3 ]; then
    pass "metrics-producer negative path: receipt collision returns exit 3"
else
    fail "metrics-producer negative path: expected exit 3, got $mrc"
fi

if [ -f "$MNEG" ] && [ ! -e "$MNEG.receipt" ] \
   && grep -q "could not write receipt" "$MNEG_ERR"; then
    pass "metrics-producer negative path: JSON survives, final receipt absent, warning emitted"
else
    fail "metrics-producer negative-path artifact/warning contract broken"
fi

m_tmp=$(printf '%s\n' "$MNEG".receipt.tmp.* | head -n 1)
if [ -f "$m_tmp" ] && [ "$(cat "$m_tmp")" = "G6_PREEXISTING_SENTINEL" ]; then
    pass "metrics-producer writer respects O_EXCL and does not clobber occupied temp"
else
    fail "metrics-producer temp-path collision was not preserved"
fi

# 6-8) TRUE negative path for arch-probe.
CNEG="$SCRATCH/c-neg.json"
CNEG_ERR="$SCRATCH/c-neg.err"
OUT="$CNEG" BIN="$PROBE" bash -c '
    tmp="${OUT}.receipt.tmp.$$"
    printf "%s\n" "G6_PREEXISTING_SENTINEL" > "$tmp"
    exec "$BIN" "$OUT"
' >/dev/null 2>"$CNEG_ERR"
crc=$?

if [ "$crc" -eq 3 ]; then
    pass "arch-probe negative path: receipt collision returns exit 3"
else
    fail "arch-probe negative path: expected exit 3, got $crc"
fi

if [ -f "$CNEG" ] && [ ! -e "$CNEG.receipt" ] \
   && grep -q "could not write" "$CNEG_ERR"; then
    pass "arch-probe negative path: JSON survives, final receipt absent, warning emitted"
else
    fail "arch-probe negative-path artifact/warning contract broken"
fi

c_tmp=$(printf '%s\n' "$CNEG".receipt.tmp.* | head -n 1)
if [ -f "$c_tmp" ] && [ "$(cat "$c_tmp")" = "G6_PREEXISTING_SENTINEL" ]; then
    pass "arch-probe writer respects O_EXCL and does not clobber occupied temp"
else
    fail "arch-probe temp-path collision was not preserved"
fi

# 9) Happy-path output SHA must match system sha256sum.
receipt_sha=$(jq -r '.outputs[0].sha256' "$SCRATCH/m.json.receipt" 2>/dev/null)
actual_sha=$(sha256sum "$SCRATCH/m.json" | cut -d' ' -f1)
if [ "$receipt_sha" = "$actual_sha" ]; then
    pass "receipt SHA proves happy-path emission matches output bytes ($actual_sha)"
else
    fail "receipt SHA mismatch on happy path — G6 regression"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
