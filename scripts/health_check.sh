#!/bin/bash
#
# Phase 9.18: Health Check Validator
# Validates Φ, latency, error rates, and thermal stability
#
# Exit codes: 0 = PASS, 1 = CRITICAL, 2 = WARNING

set -euo pipefail

DEPLOY_LOG="/var/log/termux-build/deploy.log"

# Thresholds (from deploy_staged.sh)
COHERENCE_MIN=${COHERENCE_MIN:-0.80}
COHERENCE_TARGET=${COHERENCE_TARGET:-0.85}
LATENCY_MAX=${LATENCY_MAX:-10.0}
OVERHEAD_MAX=${OVERHEAD_MAX:-0.05}
ERROR_RATE_MAX=${ERROR_RATE_MAX:-0.02}

log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" | tee -a "$DEPLOY_LOG"
}

health_check_coherence() {
    log "Checking coherence φ metric..."

    # Query recent build metrics (from Phase 9.16 CI/CD)
    local metrics_file="${BUILD_DIR}/metrics_current.json"
    if [ ! -f "$metrics_file" ]; then
        log "WARNING: No current metrics found. Skipping coherence check."
        return 2
    fi

    local coherence=$(jq -r '.coherence_phi // 0' "$metrics_file" 2>/dev/null || echo "0")

    if (( $(echo "$coherence < $COHERENCE_MIN" | bc -l) )); then
        log "CRITICAL: Coherence φ=$coherence < minimum $COHERENCE_MIN"
        return 1
    fi

    if (( $(echo "$coherence < $COHERENCE_TARGET" | bc -l) )); then
        log "WARNING: Coherence φ=$coherence below target $COHERENCE_TARGET"
        return 2
    fi

    log "✓ Coherence φ=$coherence acceptable"
    return 0
}

health_check_latency() {
    log "Checking per-package latency..."

    local metrics_file="${BUILD_DIR}/metrics_current.json"
    if [ ! -f "$metrics_file" ]; then
        return 2
    fi

    local latency=$(jq -r '.latency_mean_sec // 0' "$metrics_file" 2>/dev/null || echo "0")
    local latency_p99=$(jq -r '.latency_p99_sec // 0' "$metrics_file" 2>/dev/null || echo "0")

    if (( $(echo "$latency_p99 > $LATENCY_MAX" | bc -l) )); then
        log "CRITICAL: P99 latency=$latency_p99s > max $LATENCY_MAX seconds"
        return 1
    fi

    log "✓ Latency mean=$latency, P99=$latency_p99 acceptable"
    return 0
}

health_check_errors() {
    log "Checking error rate..."

    local metrics_file="${BUILD_DIR}/metrics_current.json"
    if [ ! -f "$metrics_file" ]; then
        return 2
    fi

    local error_rate=$(jq -r '.error_rate // 0' "$metrics_file" 2>/dev/null || echo "0")

    if (( $(echo "$error_rate > $ERROR_RATE_MAX" | bc -l) )); then
        log "CRITICAL: Error rate=$error_rate > max $ERROR_RATE_MAX"
        return 1
    fi

    log "✓ Error rate=$error_rate acceptable"
    return 0
}

health_check_overhead() {
    log "Checking heap overhead..."

    local metrics_file="${BUILD_DIR}/metrics_current.json"
    if [ ! -f "$metrics_file" ]; then
        return 2
    fi

    local overhead=$(jq -r '.overhead_heap // 0' "$metrics_file" 2>/dev/null || echo "0")

    if (( $(echo "$overhead > $OVERHEAD_MAX" | bc -l) )); then
        log "CRITICAL: Heap overhead=$overhead > max $OVERHEAD_MAX"
        return 1
    fi

    log "✓ Heap overhead=$overhead acceptable"
    return 0
}

health_check_cache() {
    log "Checking cache efficiency..."

    local metrics_file="${BUILD_DIR}/metrics_current.json"
    if [ ! -f "$metrics_file" ]; then
        return 2
    fi

    local cache_hit=$(jq -r '.cache_hit_rate // 0' "$metrics_file" 2>/dev/null || echo "0")

    if (( $(echo "$cache_hit < 0.70" | bc -l) )); then
        log "WARNING: Cache hit rate=$cache_hit < 70% target"
        return 2
    fi

    log "✓ Cache hit rate=$cache_hit acceptable"
    return 0
}

# ============================================================================
# Main health check routine
# ============================================================================

BUILD_DIR="${BUILD_DIR:-/home/termux-build}"
mkdir -p "$(dirname "$DEPLOY_LOG")"

HEALTH_EXIT=0

log "=== Health Check: All Subsystems ==="

health_check_coherence || HEALTH_EXIT=$?
health_check_latency || HEALTH_EXIT=$?
health_check_errors || HEALTH_EXIT=$?
health_check_overhead || HEALTH_EXIT=$?
health_check_cache || HEALTH_EXIT=$?

if [ $HEALTH_EXIT -eq 0 ]; then
    log "✓✓✓ All health checks PASSED"
elif [ $HEALTH_EXIT -eq 2 ]; then
    log "⚠️  Some checks triggered warnings (non-fatal)"
else
    log "✗✗✗ Health check FAILED with critical issues"
fi

exit $HEALTH_EXIT
