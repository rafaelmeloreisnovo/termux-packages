#!/bin/bash
#
# REAL: Governance gate.
# Status: OBSERVED_LIMITED — enforcement of contract + provenance + regression.
#
# What this enforces (all fail-closed):
#   1) Producer/validators exist and execute.
#   2) Current JSON passes duplicate-aware, scope-aware strict validation.
#   3) Current JSON also passes the compact C pkg_metrics validator.
#   4) TOKEN_VAZIO cannot leak into the promoted metrics artifact.
#   5) Baseline exists and passes the same strict structural contract.
#   6) Baseline exposes the fields used by the regression policy.
#   7) Graph metrics satisfy the declared regression budget.
#
# Important scope boundary:
#   Passing this gate validates pkg_metrics/1.0.0 plus its regression policy.
#   It does NOT prove package buildability, Android/device runtime,
#   cross-architecture portability, cryptographic security, or product readiness.
#
# Exit codes: 0 = PASS, 1 = FAIL/BLOCKED.

set -uo pipefail

REPO_ROOT="${REPO_ROOT:-.}"
BASELINE="${BASELINE:-core/tests/fixtures/real_dag_baseline.json}"
PRODUCER="${PRODUCER:-core/metrics-producer}"
VALIDATOR="${VALIDATOR:-core/contract-validate}"
STRICT_VALIDATOR="${STRICT_VALIDATOR:-scripts/validate_pkg_metrics_json.py}"
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

require_json_file() {
    local file="$1"
    [ -f "$file" ] || fail "required JSON missing: $file (TOKEN_VAZIO_SOURCE)"
    jq -e . "$file" >/dev/null 2>&1 || fail "invalid JSON: $file"
}

require_json_field() {
    local file="$1" field="$2"
    if ! jq -e ".$field != null" "$file" >/dev/null 2>&1; then
        fail "$file missing required field: $field"
    fi
}

require_no_token_vazio() {
    local file="$1"
    if grep -q "TOKEN_VAZIO" "$file"; then
        grep "TOKEN_VAZIO" "$file" >&2 || true
        fail "$file contains TOKEN_VAZIO placeholders"
    fi
}

strict_validate() {
    local file="$1"
    if ! python3 "$STRICT_VALIDATOR" "$file" >/dev/null; then
        fail "strict duplicate/scope-aware contract rejected: $file"
    fi
}

log "=== pkg_metrics Governance Gate ==="
log "repo_root=$REPO_ROOT"
log "baseline=$BASELINE"
log "producer=$PRODUCER"
log "strict_validator=$STRICT_VALIDATOR"

for cmd in jq python3; do
    command -v "$cmd" >/dev/null 2>&1 || fail "$cmd not installed"
done
[ -f "$STRICT_VALIDATOR" ] || fail "strict validator missing: $STRICT_VALIDATOR"

# 1) Binaries present
require_bin "$PRODUCER"
require_bin "$VALIDATOR"

# 2) Producer runs
log "step 1/7: run producer"
if ! "$PRODUCER" "$REPO_ROOT" "$OUT_JSON" >/dev/null; then
    fail "producer failed to run"
fi

# 3) Strict JSON grammar/scope/duplicate gate
log "step 2/7: strict current JSON validation"
require_json_file "$OUT_JSON"
strict_validate "$OUT_JSON"

# 4) Compact C contract validation remains an independent ruler
log "step 3/7: C contract validation (pkg_metrics/1.0.0)"
if ! "$VALIDATOR" "$OUT_JSON"; then
    fail "C contract validation rejected the JSON"
fi

# 5) No TOKEN_VAZIO leaks into the promoted metrics artifact
log "step 4/7: scan current output for TOKEN_VAZIO"
require_no_token_vazio "$OUT_JSON"

# 6) Baseline is evidence too and must satisfy the strict contract.
log "step 5/7: strict regression baseline validation"
require_json_file "$BASELINE"
strict_validate "$BASELINE"
require_no_token_vazio "$BASELINE"

# Explicitly name the subset consumed by regression arithmetic.
log "step 6/7: regression field presence"
for field in node_count edge_count coherence_phi cycle_count unresolved_count; do
    require_json_field "$BASELINE" "$field"
    require_json_field "$OUT_JSON" "$field"
done

# 7) Regression gate vs baseline
log "step 7/7: regression gate vs $BASELINE"
base_nodes=$(jq -r '.node_count' "$BASELINE")
base_edges=$(jq -r '.edge_count' "$BASELINE")
base_phi=$(jq -r '.coherence_phi' "$BASELINE")
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
log "STRICT_JSON_GATE=PASS duplicate_keys=REJECTED scope_enforcement=PASS"
log "✓✓✓ pkg_metrics governance gate PASSED (scope-limited; not product readiness)"
exit 0
