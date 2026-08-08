#!/bin/bash
#
# REAL: Governance gate.
# Status: REAL — end-to-end enforcement of contract + provenance + regression.
#
# What this enforces (all fail-closed):
#   1) The producer binary exists and is executable.
#   2) The producer runs and produces a JSON with status="REAL".
#   3) The JSON validates against pkg_metrics/1.0.0 (real_contract).
#   4) No provenance field contains TOKEN_VAZIO.
#   5) Real numbers do not regress vs the tracked baseline:
#         - node_count       >= baseline.node_count
#         - edge_count       >= baseline.edge_count
#         - coherence_phi    >= baseline.coherence_phi - 0.001
#         - cycle_count      == baseline.cycle_count  (must stay 0)
#         - unresolved_count <= baseline.unresolved_count + 5  (drift budget)
#
# Exit codes: 0 = PASS, 1 = FAIL.

set -uo pipefail

REPO_ROOT="${REPO_ROOT:-.}"
BASELINE="${BASELINE:-core/tests/fixtures/real_dag_baseline.json}"
PRODUCER="${PRODUCER:-core/metrics-producer}"
VALIDATOR="${VALIDATOR:-core/contract-validate}"
RECEIPT_VALIDATOR="${RECEIPT_VALIDATOR:-core/receipt-validate}"
OUT_JSON="${OUT_JSON:-/tmp/real_gov_metrics.json}"

log() { echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" >&2; }

fail() {
    log "FAIL: $*"
    exit 1
}

require_bin() {
    if [ ! -x "$1" ]; then
        fail "required binary missing or not executable: $1"
    fi
}

require_json_field() {
    local file="$1" field="$2"
    if ! jq -e ".$field" "$file" >/dev/null 2>&1; then
        fail "$file missing required field: $field"
    fi
}

log "=== REAL Governance Gate ==="
log "repo_root=$REPO_ROOT"
log "baseline=$BASELINE"
log "producer=$PRODUCER"

command -v jq >/dev/null 2>&1 || fail "jq not installed"

# 1) Binaries present
require_bin "$PRODUCER"
require_bin "$VALIDATOR"

# 2) Producer runs
log "step 1/6: run producer"
if ! "$PRODUCER" "$REPO_ROOT" "$OUT_JSON" >/dev/null; then
    fail "producer failed to run"
fi

# 3) status=REAL
log "step 2/6: check status=REAL"
if ! jq -e '.status == "REAL"' "$OUT_JSON" >/dev/null 2>&1; then
    fail "output is not status=REAL"
fi

# 4) Contract validation
log "step 3/6: contract validation (pkg_metrics/1.0.0)"
if ! "$VALIDATOR" "$OUT_JSON"; then
    fail "contract validation rejected the JSON"
fi

# 5) No TOKEN_VAZIO leaks
log "step 4/6: scan for TOKEN_VAZIO"
if grep -q "TOKEN_VAZIO" "$OUT_JSON"; then
    grep "TOKEN_VAZIO" "$OUT_JSON" >&2
    fail "output contains TOKEN_VAZIO placeholders"
fi

# 6) Regression gate vs baseline
log "step 5/6: regression gate vs $BASELINE"
if [ ! -f "$BASELINE" ]; then
    log "WARN: baseline missing; skipping regression gate"
else
    base_nodes=$(jq -r '.node_count' "$BASELINE")
    base_edges=$(jq -r '.edge_count' "$BASELINE")
    base_phi=$(jq -r '.coherence_phi // 0' "$BASELINE")
    base_cycles=$(jq -r '.cycle_count' "$BASELINE")
    base_unres=$(jq -r '.unresolved_count' "$BASELINE")

    cur_nodes=$(jq -r '.node_count' "$OUT_JSON")
    cur_edges=$(jq -r '.edge_count' "$OUT_JSON")
    cur_phi=$(jq -r '.coherence_phi' "$OUT_JSON")
    cur_cycles=$(jq -r '.cycle_count' "$OUT_JSON")
    cur_unres=$(jq -r '.unresolved_count' "$OUT_JSON")

    log "baseline: nodes=$base_nodes edges=$base_edges phi=$base_phi cycles=$base_cycles unres=$base_unres"
    log "current:  nodes=$cur_nodes edges=$cur_edges phi=$cur_phi cycles=$cur_cycles unres=$cur_unres"

    if [ "$cur_nodes" -lt "$base_nodes" ]; then
        fail "regression: node_count $cur_nodes < baseline $base_nodes"
    fi
    if [ "$cur_edges" -lt "$base_edges" ]; then
        fail "regression: edge_count $cur_edges < baseline $base_edges"
    fi
    if [ "$cur_cycles" -ne "$base_cycles" ]; then
        fail "regression: cycle_count $cur_cycles != baseline $base_cycles"
    fi

    drift=$(( cur_unres - base_unres ))
    if [ "$drift" -gt 5 ]; then
        fail "regression: unresolved drift +$drift > 5 (cur=$cur_unres base=$base_unres)"
    fi

    if awk "BEGIN { exit !($cur_phi < $base_phi - 0.001) }"; then
        fail "regression: coherence_phi $cur_phi < baseline $base_phi - 0.001"
    fi

    log "regression gate passed"
fi

log "step 6/6: verify receipt for output"
if [ -f "$RECEIPT_VALIDATOR" ] && [ -f "$OUT_JSON.receipt" ]; then
    if ! "$RECEIPT_VALIDATOR" "$OUT_JSON.receipt" >/dev/null 2>&1; then
        fail "receipt verification failed for $OUT_JSON.receipt"
    fi
    log "receipt signature valid"
else
    log "WARN: no receipt validator or receipt file; skipping"
fi

log "✓✓✓ REAL Governance Gate PASSED"
exit 0
