#!/usr/bin/env bash
# REAL: end-to-end governance test — provenance + contract + regression.
# Status: OBSERVED_LIMITED — exercises producer/validator/gate on current host.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

REPO_ROOT="."
PROD="core/metrics-producer"
VAL="core/contract-validate"
SCRATCH="/tmp/real_gov_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

echo "=== REAL: governance end-to-end ==="

# 1) Produce real metrics
if "$PROD" "$REPO_ROOT" "$SCRATCH/m.json" >/dev/null 2>&1; then
    pass "producer emits JSON"
else
    fail "producer failed"; exit 1
fi

# 2) status=REAL present
if grep -q '"status": "REAL"' "$SCRATCH/m.json"; then
    pass "JSON has status=REAL"
else
    fail "no status=REAL"
fi

# 3) schema is pkg_metrics/1.0.0
if grep -q '"schema": "pkg_metrics/1.0.0"' "$SCRATCH/m.json"; then
    pass "schema=pkg_metrics/1.0.0"
else
    fail "wrong schema"
fi

# 4) provenance fields present (all non-empty, no TOKEN_VAZIO)
for f in git_commit build_timestamp_utc toolchain_id host_uname producer_name; do
    if grep -qE "\"$f\": \"[^\"]+\"" "$SCRATCH/m.json" && \
       ! grep -qE "\"$f\": \"TOKEN_VAZIO" "$SCRATCH/m.json"; then
        pass "provenance.$f present + no TOKEN_VAZIO"
    else
        fail "provenance.$f missing or has TOKEN_VAZIO"
    fi
done

# 5) contract validation passes
if "$VAL" "$SCRATCH/m.json" >/dev/null 2>&1; then
    pass "contract validation passes on real JSON"
else
    fail "contract validation rejected real JSON"
fi

# 6) negative test — status=SIMULATED must be rejected
sed 's/"status": "REAL"/"status": "SIMULATED"/' "$SCRATCH/m.json" > "$SCRATCH/n1.json"
if ! "$VAL" "$SCRATCH/n1.json" >/dev/null 2>&1; then
    pass "negative: SIMULATED rejected"
else
    fail "negative: SIMULATED accepted"
fi

# 7) negative test — TOKEN_VAZIO in git_commit
sed 's/"git_commit": "[^"]*"/"git_commit": "TOKEN_VAZIO_x"/' "$SCRATCH/m.json" > "$SCRATCH/n2.json"
if ! "$VAL" "$SCRATCH/n2.json" >/dev/null 2>&1; then
    pass "negative: TOKEN_VAZIO in provenance rejected"
else
    fail "negative: TOKEN_VAZIO accepted"
fi

# 8) negative test — coherence_phi > 1.0
sed 's/"coherence_phi": [^,]*/"coherence_phi": 1.5/' "$SCRATCH/m.json" > "$SCRATCH/n3.json"
if ! "$VAL" "$SCRATCH/n3.json" >/dev/null 2>&1; then
    pass "negative: coherence_phi>1.0 rejected"
else
    fail "negative: coherence_phi>1.0 accepted"
fi

# 9) negative test — cycles>0 while acyclicity=1
sed 's/"cycle_count": 0/"cycle_count": 5/' "$SCRATCH/m.json" > "$SCRATCH/n4.json"
if ! "$VAL" "$SCRATCH/n4.json" >/dev/null 2>&1; then
    pass "negative: cycles>0 with acyclicity=1 rejected"
else
    fail "negative: cycles>0 with acyclicity=1 accepted"
fi

# 10) governance gate end-to-end
if bash scripts/real_governance.sh >/dev/null 2>&1; then
    pass "governance gate PASS end-to-end"
else
    fail "governance gate FAIL"
fi

# 11) negative — missing baseline must block, never skip regression
if ! BASELINE="$SCRATCH/does-not-exist.json" \
     bash scripts/real_governance.sh >/dev/null 2>&1; then
    pass "negative: missing baseline blocks gate"
else
    fail "negative: missing baseline was silently accepted"
fi

# 12) negative — invalid baseline JSON must block
printf '{broken\n' > "$SCRATCH/bad-baseline.json"
if ! BASELINE="$SCRATCH/bad-baseline.json" \
     bash scripts/real_governance.sh >/dev/null 2>&1; then
    pass "negative: invalid baseline JSON blocks gate"
else
    fail "negative: invalid baseline JSON accepted"
fi

# 13) negative — non-REAL baseline must block
cp core/tests/fixtures/real_dag_baseline.json "$SCRATCH/sim-baseline.json"
sed -i 's/"status": "REAL"/"status": "SIMULATED"/' "$SCRATCH/sim-baseline.json"
if ! BASELINE="$SCRATCH/sim-baseline.json" \
     bash scripts/real_governance.sh >/dev/null 2>&1; then
    pass "negative: non-REAL baseline blocks gate"
else
    fail "negative: non-REAL baseline accepted"
fi

# 14) negative — TOKEN_VAZIO in baseline provenance must block
cp core/tests/fixtures/real_dag_baseline.json "$SCRATCH/tv-baseline.json"
sed -i 's/"git_commit": "[^"]*"/"git_commit": "TOKEN_VAZIO_baseline"/' "$SCRATCH/tv-baseline.json"
if ! BASELINE="$SCRATCH/tv-baseline.json" \
     bash scripts/real_governance.sh >/dev/null 2>&1; then
    pass "negative: TOKEN_VAZIO baseline blocks gate"
else
    fail "negative: TOKEN_VAZIO baseline accepted"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
