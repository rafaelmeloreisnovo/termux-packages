#!/bin/bash
#
# REAL: Health Check Validator (fail-closed)
# Status: REAL — every branch fails on missing/invalid data instead of
# silently warning. Requires metrics_current.json with status=REAL.
#
# Exit codes: 0 = PASS, 1 = CRITICAL, 2 = WARNING
#
# Metric fields consumed (all REAL, produced by core/metrics-producer):
#   coherence_phi        — graph-derived Φ
#   graph_completeness   — 1 - unresolved/edges
#   graph_acyclicity     — 1 - cycles/nodes
#   avg_deps_per_pkg     — edges/nodes
#   inventory_latency_us — scan wall time
#   dag_latency_us       — DAG build+topo wall time

set -euo pipefail

DEPLOY_LOG="${DEPLOY_LOG:-/var/log/termux-build/deploy.log}"
BUILD_DIR="${BUILD_DIR:-/home/termux-build}"
METRICS_FILE="${BUILD_DIR}/metrics_current.json"

# Thresholds
COHERENCE_MIN=${COHERENCE_MIN:-0.80}
COHERENCE_TARGET=${COHERENCE_TARGET:-0.85}
LATENCY_MAX_US=${LATENCY_MAX_US:-10000000}       # 10s in us
COMPLETENESS_MIN=${COMPLETENESS_MIN:-0.95}       # ≤5% unresolved acceptable

mkdir -p "$(dirname "$DEPLOY_LOG")" 2>/dev/null || true

log() { echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" | tee -a "$DEPLOY_LOG" >&2; }

# ============================================================================
# REAL: single fail-closed gate on metrics file
# Returns 0 if metrics file exists, is valid JSON, and status=REAL.
# Returns 1 (CRITICAL) otherwise — no silent fallback.
# ============================================================================
require_real_metrics() {
    if [ ! -f "$METRICS_FILE" ]; then
        log "CRITICAL: metrics_current.json missing at $METRICS_FILE (fail-closed)"
        return 1
    fi
    if ! command -v jq >/dev/null 2>&1; then
        log "CRITICAL: jq not installed — cannot validate metrics (fail-closed)"
        return 1
    fi
    if ! jq -e . "$METRICS_FILE" >/dev/null 2>&1; then
        log "CRITICAL: metrics_current.json is not valid JSON (fail-closed)"
        return 1
    fi
    if ! jq -e '.status == "REAL"' "$METRICS_FILE" >/dev/null 2>&1; then
        log "CRITICAL: metrics_current.json is not status=REAL (fail-closed)"
        return 1
    fi
    return 0
}

# jq_or_fail <path> — returns numeric value or 1 if field missing/null.
jq_or_fail() {
    local path="$1"
    local raw
    raw=$(jq -r "$path // \"MISSING\"" "$METRICS_FILE" 2>/dev/null || echo "MISSING")
    if [ "$raw" = "MISSING" ] || [ "$raw" = "null" ]; then
        log "CRITICAL: metric field $path missing (fail-closed)"
        return 1
    fi
    echo "$raw"
    return 0
}

# ============================================================================
# REAL health checks — all consume the validated file, no per-check re-check
# ============================================================================

health_check_coherence() {
    log "Checking coherence φ..."
    local coherence
    coherence=$(jq_or_fail '.coherence_phi') || return 1

    if (( $(echo "$coherence < $COHERENCE_MIN" | bc -l) )); then
        log "CRITICAL: coherence φ=$coherence < min $COHERENCE_MIN"
        return 1
    fi
    if (( $(echo "$coherence < $COHERENCE_TARGET" | bc -l) )); then
        log "WARNING: coherence φ=$coherence below target $COHERENCE_TARGET"
        return 2
    fi
    log "✓ coherence φ=$coherence acceptable"
    return 0
}

health_check_completeness() {
    log "Checking graph completeness..."
    local completeness
    completeness=$(jq_or_fail '.graph_completeness') || return 1
    if (( $(echo "$completeness < $COMPLETENESS_MIN" | bc -l) )); then
        log "WARNING: completeness=$completeness < min $COMPLETENESS_MIN"
        return 2
    fi
    log "✓ completeness=$completeness acceptable"
    return 0
}

health_check_acyclicity() {
    log "Checking graph acyclicity..."
    local cycles
    cycles=$(jq_or_fail '.cycle_count') || return 1
    if [ "$cycles" -ne 0 ]; then
        log "CRITICAL: $cycles cycles detected in real DAG (fail-closed)"
        return 1
    fi
    log "✓ acyclic (0 cycles across real DAG)"
    return 0
}

health_check_latency() {
    log "Checking build latency..."
    local total_us
    total_us=$(jq_or_fail '.total_latency_us') || return 1
    if [ "$total_us" -gt "$LATENCY_MAX_US" ]; then
        log "CRITICAL: total_latency_us=$total_us > max $LATENCY_MAX_US"
        return 1
    fi
    log "✓ total_latency_us=$total_us acceptable"
    return 0
}

# ============================================================================
# Main — fail fast on missing file, run each check exactly once
# ============================================================================

log "=== REAL Health Check ==="
log "metrics=$METRICS_FILE"

# Single fail-closed gate — if this fails, none of the checks run.
if ! require_real_metrics; then
    log "✗✗✗ Health check ABORTED (real metrics not available)"
    exit 1
fi

HEALTH_EXIT=0
for fn in health_check_coherence health_check_completeness \
          health_check_acyclicity health_check_latency; do
    "$fn" || rc=$?
    rc=${rc:-0}
    if [ "$rc" -gt "$HEALTH_EXIT" ]; then HEALTH_EXIT=$rc; fi
    unset rc
done

case "$HEALTH_EXIT" in
    0) log "✓✓✓ All REAL health checks PASSED" ;;
    2) log "⚠️  Some checks triggered warnings (non-fatal)" ;;
    *) log "✗✗✗ Health check FAILED with critical issues" ;;
esac
exit $HEALTH_EXIT
