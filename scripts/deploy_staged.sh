#!/bin/bash
#
# Phase 9.18: Staged Deployment Controller
# Rollout gradual: staging → 10% → 50% → 100% com health checks
#
# Usage: ./deploy_staged.sh [stage0|stage1|stage2|stage3|rollback]

set -euo pipefail

DEPLOY_LOG="/var/log/termux-build/deploy.log"
HEALTH_CHECK_SCRIPT="./scripts/health_check.sh"
ROLLBACK_SCRIPT="./scripts/rollback.sh"
METRICS_EXPORT="./scripts/metrics_export.sh"

# Configuration
DEPLOY_BRANCH="claude/sistema-nucleo-autoral-2bju50"
BUILD_DIR="/home/termux-build"
CHECKPOINT_DIR="${BUILD_DIR}/_checkpoints"
BACKUP_DIR="/backup/termux-build-$(date +%Y%m%d-%H%M%S)"

# Health check thresholds
COHERENCE_MIN=0.80
COHERENCE_TARGET=0.85
LATENCY_MAX=10.0
OVERHEAD_MAX=0.05
ERROR_RATE_MAX=0.02

mkdir -p "$(dirname "$DEPLOY_LOG")"

log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" | tee -a "$DEPLOY_LOG"
}

error() {
    echo "[ERROR] $*" | tee -a "$DEPLOY_LOG"
    exit 1
}

# ============================================================================
# Stage 0: Staging Environment (Pre-Production Testing)
# ============================================================================

stage0_staging() {
    log "=== Stage 0: Staging Deployment ==="

    # Backup current production
    log "Backing up current production to ${BACKUP_DIR}"
    mkdir -p "$BACKUP_DIR"
    cp -r "${BUILD_DIR}/core" "${BACKUP_DIR}/" || true

    # Deploy to staging environment (isolated)
    log "Deploying Phase 9.15/9.16/3 to staging"
    git clone --branch "$DEPLOY_BRANCH" https://github.com/rafaelmeloreisnovo/termux-packages.git \
        "${BUILD_DIR}/staging" 2>&1 | tee -a "$DEPLOY_LOG" || error "Clone failed"

    cd "${BUILD_DIR}/staging/core"

    # Compile and run all tests
    log "Compiling tests in staging"
    make clean test-production-hardening test-orchestrator-v2 2>&1 | tee -a "$DEPLOY_LOG" || error "Compilation failed"

    log "Running tests in staging"
    ./test-production-hardening 2>&1 | tee -a "$DEPLOY_LOG" || error "test-production-hardening failed"
    ./test-orchestrator-v2 2>&1 | tee -a "$DEPLOY_LOG" || error "test-orchestrator-v2 failed"

    log "✓ Stage 0 (Staging) deployment successful"
    log "Next: Run 'deploy_staged.sh stage1' to proceed to 10% canary"
}

# ============================================================================
# Stage 1: Canary Deployment (10% of jobs)
# ============================================================================

stage1_canary_10() {
    log "=== Stage 1: Canary Deployment (10% traffic) ==="

    if [ ! -d "${BUILD_DIR}/staging" ]; then
        error "Staging environment not found. Run 'deploy_staged.sh stage0' first"
    fi

    # Copy staged code to production (controlled environment)
    log "Copying staged code to production (10% slice)"
    cp -r "${BUILD_DIR}/staging/core/build_orchestrator_v2.*" "${BUILD_DIR}/core/"
    cp -r "${BUILD_DIR}/staging/core/dep_resolver_v2.*" "${BUILD_DIR}/core/"

    # Update CI to route 10% of jobs to Phase 3
    log "Updating CI/CD for 10% canary (set CI_PHASE3_ENABLE=0.1)"
    export CI_PHASE3_ENABLE=0.1

    log "Running health checks"
    if ! bash "$HEALTH_CHECK_SCRIPT"; then
        error "Health check failed in canary"
    fi

    log "Collecting metrics for 10% canary"
    bash "$METRICS_EXPORT" > "${BUILD_DIR}/metrics_stage1.json"

    log "✓ Stage 1 (10% Canary) deployment successful"
    log "Monitor metrics for 2-4 hours before proceeding to Stage 2"
    log "Next: Run 'deploy_staged.sh stage2' to proceed to 50% rollout"
}

# ============================================================================
# Stage 2: Progressive Rollout (50% of jobs)
# ============================================================================

stage2_progressive_50() {
    log "=== Stage 2: Progressive Rollout (50% traffic) ==="

    # Verify Stage 1 metrics are acceptable
    log "Verifying Stage 1 metrics"
    if [ ! -f "${BUILD_DIR}/metrics_stage1.json" ]; then
        error "Stage 1 metrics not found. Please complete Stage 1 first"
    fi

    # Quick check: parse coherence from metrics
    COHERENCE=$(jq -r '.coherence_phi' "${BUILD_DIR}/metrics_stage1.json" 2>/dev/null || echo "0")
    if (( $(echo "$COHERENCE < $COHERENCE_MIN" | bc -l) )); then
        error "Stage 1 coherence φ=$COHERENCE below minimum $COHERENCE_MIN. Aborting rollout."
    fi

    log "Stage 1 metrics acceptable (Φ=$COHERENCE)"

    # Update CI to route 50% of jobs to Phase 3
    log "Updating CI/CD for 50% rollout (set CI_PHASE3_ENABLE=0.5)"
    export CI_PHASE3_ENABLE=0.5

    log "Running health checks"
    if ! bash "$HEALTH_CHECK_SCRIPT"; then
        error "Health check failed at 50% rollout"
    fi

    log "Collecting metrics for 50% rollout"
    bash "$METRICS_EXPORT" > "${BUILD_DIR}/metrics_stage2.json"

    log "✓ Stage 2 (50% Progressive) deployment successful"
    log "Next: Run 'deploy_staged.sh stage3' to proceed to 100% production"
}

# ============================================================================
# Stage 3: Full Production (100% of jobs)
# ============================================================================

stage3_production_100() {
    log "=== Stage 3: Full Production Deployment (100% traffic) ==="

    # Verify Stage 2 metrics are acceptable
    log "Verifying Stage 2 metrics"
    if [ ! -f "${BUILD_DIR}/metrics_stage2.json" ]; then
        error "Stage 2 metrics not found. Please complete Stage 2 first"
    fi

    COHERENCE=$(jq -r '.coherence_phi' "${BUILD_DIR}/metrics_stage2.json" 2>/dev/null || echo "0")
    if (( $(echo "$COHERENCE < $COHERENCE_MIN" | bc -l) )); then
        error "Stage 2 coherence φ=$COHERENCE below minimum. Aborting production rollout."
    fi

    log "Stage 2 metrics acceptable (Φ=$COHERENCE)"

    # Final production update
    log "Deploying Phase 9.15/9.16/3 to 100% production"
    export CI_PHASE3_ENABLE=1.0

    log "Running final health checks"
    if ! bash "$HEALTH_CHECK_SCRIPT"; then
        error "Health check failed at 100% production"
    fi

    log "Collecting final production metrics"
    bash "$METRICS_EXPORT" > "${BUILD_DIR}/metrics_production.json"

    log "✓ Stage 3 (100% Production) deployment successful"
    log "=== DEPLOYMENT COMPLETE ==="
    log "All systems operational. Phase 9.15/9.16/3 now in production."
}

# ============================================================================
# Rollback (Revert to previous version)
# ============================================================================

rollback_to_previous() {
    log "=== ROLLBACK INITIATED ==="

    if [ -z "$BACKUP_DIR" ] || [ ! -d "$BACKUP_DIR" ]; then
        error "Backup directory not found. Cannot rollback."
    fi

    log "Restoring from backup: $BACKUP_DIR"
    bash "$ROLLBACK_SCRIPT" "$BACKUP_DIR"

    log "Resetting CI/CD to Phase 9.14"
    export CI_PHASE3_ENABLE=0.0

    log "Running health checks post-rollback"
    if bash "$HEALTH_CHECK_SCRIPT"; then
        log "✓ Rollback successful. System reverted to previous version."
    else
        error "Rollback health check failed. Manual intervention required."
    fi
}

# ============================================================================
# Main entrypoint
# ============================================================================

STAGE="${1:-}"

case "$STAGE" in
    stage0|staging)
        stage0_staging
        ;;
    stage1|canary|10%)
        stage1_canary_10
        ;;
    stage2|progressive|50%)
        stage2_progressive_50
        ;;
    stage3|production|100%)
        stage3_production_100
        ;;
    rollback)
        rollback_to_previous
        ;;
    status)
        log "=== Deployment Status ==="
        [ -f "${BUILD_DIR}/metrics_stage1.json" ] && log "Stage 1: $(jq -r '.coherence_phi' "${BUILD_DIR}/metrics_stage1.json")"
        [ -f "${BUILD_DIR}/metrics_stage2.json" ] && log "Stage 2: $(jq -r '.coherence_phi' "${BUILD_DIR}/metrics_stage2.json")"
        [ -f "${BUILD_DIR}/metrics_production.json" ] && log "Production: $(jq -r '.coherence_phi' "${BUILD_DIR}/metrics_production.json")"
        ;;
    *)
        cat << 'EOF'
Usage: ./deploy_staged.sh [STAGE]

Stages (in order):
  stage0, staging     - Deploy to staging environment
  stage1, canary, 10% - Canary deployment (10% traffic)
  stage2, progressive, 50% - Progressive rollout (50% traffic)
  stage3, production, 100% - Full production (100% traffic)

Commands:
  rollback           - Revert to previous version
  status             - Show deployment status

Example:
  ./deploy_staged.sh stage0
  # Monitor for 2-4 hours
  ./deploy_staged.sh stage1
  # Monitor for 4-8 hours
  ./deploy_staged.sh stage2
  # Monitor for 4-8 hours
  ./deploy_staged.sh stage3
EOF
        exit 0
        ;;
esac

log "Stage: $STAGE - Done"
