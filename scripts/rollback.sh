#!/bin/bash
#
# Phase 9.18: Automated Rollback to Previous Version
# Restores build system from backup on deployment failure
#
# Usage: ./rollback.sh [BACKUP_DIR]

set -euo pipefail

DEPLOY_LOG="/var/log/termux-build/deploy.log"

log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" | tee -a "$DEPLOY_LOG"
}

error() {
    echo "[ERROR] $*" | tee -a "$DEPLOY_LOG"
    exit 1
}

if [ $# -lt 1 ]; then
    error "Usage: $0 BACKUP_DIR"
fi

BACKUP_DIR="$1"
BUILD_DIR="${BUILD_DIR:-/home/termux-build}"

log "=== ROLLBACK INITIATED ==="

if [ -z "$BACKUP_DIR" ] || [ ! -d "$BACKUP_DIR" ]; then
    error "Backup directory not found: $BACKUP_DIR"
fi

# Verify backup structure
if [ ! -d "${BACKUP_DIR}/core" ]; then
    error "Backup missing core directory: ${BACKUP_DIR}/core"
fi

log "Step 1: Stopping active build processes"
pkill -9 build-package.sh || true
pkill -9 termux-build-core || true
sleep 1

log "Step 2: Removing corrupted production build"
rm -rf "${BUILD_DIR}/core" || true

log "Step 3: Restoring from backup: $BACKUP_DIR"
if ! cp -r "${BACKUP_DIR}/core" "${BUILD_DIR}/"; then
    error "Failed to restore from backup"
fi

log "Step 4: Resetting CI/CD to Phase 9.14"
if [ -f "${BUILD_DIR}/ci_config.env" ]; then
    sed -i 's/CI_PHASE3_ENABLE=.*/CI_PHASE3_ENABLE=0.0/' "${BUILD_DIR}/ci_config.env"
fi
export CI_PHASE3_ENABLE=0.0

log "Step 5: Clearing build cache"
rm -rf "${BUILD_DIR}/_build_cache" || true
rm -rf "${BUILD_DIR}/_checkpoints" || true

log "Step 6: Running health checks post-rollback"
if ! bash "${BUILD_DIR}/../scripts/health_check.sh"; then
    log "WARNING: Post-rollback health check triggered warnings"
fi

log "✓✓✓ Rollback completed successfully"
log "System reverted to Phase 9.14 (pre-Phase 3 Orchestrator)"
log "Next: Investigate failure cause before re-attempting deployment"
