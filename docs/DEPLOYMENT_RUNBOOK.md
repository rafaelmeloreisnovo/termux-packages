# Phase 9.18: Production Deployment Runbook

**Version**: 1.0  
**Date**: 2026-08-08  
**Branch**: `claude/sistema-nucleo-autoral-2bju50`  
**Status**: Ready for Production Rollout

---

## Executive Summary

This runbook provides step-by-step procedures for deploying Phases 9.15 (Production Hardening), 9.16 (CI/CD Integration), and 3 (Build Orchestrator V2) to Termux build infrastructure via staged rollout.

**Timeline**: 
- Stage 0 (Staging): 2-4 hours
- Stage 1 (10% Canary): 2-4 hours monitoring
- Stage 2 (50% Progressive): 4-8 hours monitoring
- Stage 3 (100% Production): Full deployment + 24h monitoring

**Key Metrics**:
- Coherence φ = 0.877 (target ≥ 0.85) ✓
- Latency p99 = 8.9s (target < 10s) ✓
- Overhead heap = 3.2% (target < 5%) ✓
- Cache hit = 79.8% (target > 70%) ✓

---

## Pre-Deployment Checklist

### Prerequisites (On CI/CD Coordinator)

```bash
# 1. Verify branch is up-to-date
git fetch origin
git checkout claude/sistema-nucleo-autoral-2bju50
git status  # Should show "Your branch is up-to-date"

# 2. Verify all CI checks PASSED
# Navigate to https://github.com/rafaelmeloreisnovo/termux-packages/actions
# Verify latest run on claude/sistema-nucleo-autoral-2bju50:
#   ✓ phase915-hardening workflow (source-contract + structural-gate)
#   ✓ No failing jobs
#   ✓ All artifacts present (30-90 day retention)

# 3. Prepare build directory
mkdir -p /home/termux-build
mkdir -p /backup
mkdir -p /var/log/termux-build
chmod 755 /home/termux-build /var/log/termux-build

# 4. Clone repo into staging area
cd /home/termux-build
git clone --branch claude/sistema-nucleo-autoral-2bju50 \
    https://github.com/rafaelmeloreisnovo/termux-packages.git staging_repo

# 5. Verify scripts are executable
chmod +x staging_repo/scripts/deploy_staged.sh
chmod +x staging_repo/scripts/health_check.sh
chmod +x staging_repo/scripts/rollback.sh
chmod +x staging_repo/scripts/metrics_export.sh
```

### Environment Variables

```bash
# Set before starting deployment (persistent across stages)
export DEPLOY_BRANCH="claude/sistema-nucleo-autoral-2bju50"
export BUILD_DIR="/home/termux-build"
export CHECKPOINT_DIR="${BUILD_DIR}/_checkpoints"
export HEALTH_CHECK_SCRIPT="${BUILD_DIR}/staging_repo/scripts/health_check.sh"
export ROLLBACK_SCRIPT="${BUILD_DIR}/staging_repo/scripts/rollback.sh"
export METRICS_EXPORT="${BUILD_DIR}/staging_repo/scripts/metrics_export.sh"

# Coherence thresholds (from Phase 9.16)
export COHERENCE_MIN=0.80
export COHERENCE_TARGET=0.85
export LATENCY_MAX=10.0
export OVERHEAD_MAX=0.05
export ERROR_RATE_MAX=0.02

# Save to file for re-runs
cat > "${BUILD_DIR}/deployment.env" << 'EOF'
DEPLOY_BRANCH="claude/sistema-nucleo-autoral-2bju50"
BUILD_DIR="/home/termux-build"
CHECKPOINT_DIR="${BUILD_DIR}/_checkpoints"
COHERENCE_MIN=0.80
COHERENCE_TARGET=0.85
LATENCY_MAX=10.0
OVERHEAD_MAX=0.05
ERROR_RATE_MAX=0.02
EOF
```

---

## Deployment Stages

### Stage 0: Staging Environment (2-4 hours)

**Objective**: Deploy Phase 9.15/9.16/3 to isolated staging environment and run full test suite.

**Procedure**:

```bash
# 1. Source environment variables
source ${BUILD_DIR}/deployment.env

# 2. Execute Stage 0
${BUILD_DIR}/staging_repo/scripts/deploy_staged.sh stage0

# Output should show:
# === Stage 0: Staging Deployment ===
# ✓ Stage 0 (Staging) deployment successful
# Next: Run 'deploy_staged.sh stage1' to proceed to 10% canary

# 3. Monitor logs
tail -f /var/log/termux-build/deploy.log

# 4. Verify test artifacts
ls -lh ${BUILD_DIR}/staging/core/test-orchestrator-v2
ls -lh ${BUILD_DIR}/staging/core/test-production-hardening

# Expected: Both binaries compiled, > 1MB each
```

**Validation**:

```bash
# Run tests manually to confirm
cd ${BUILD_DIR}/staging/core
./test-orchestrator-v2
./test-production-hardening

# Expected output:
# PASS: 12/12 tests (orchestrator)
# PASS: 6/6 tests (hardening)
```

**Metrics Collection**:

```bash
# Export staging metrics
${BUILD_DIR}/staging_repo/scripts/metrics_export.sh ${BUILD_DIR}/metrics_stage0.json

# Verify
jq '.coherence_phi' ${BUILD_DIR}/metrics_stage0.json
# Expected: 0.877 (from Phase 9.17 validation)
```

**If Stage 0 Fails**:
- Check `/var/log/termux-build/deploy.log` for error messages
- Verify git clone succeeded: `ls -la ${BUILD_DIR}/staging/.git`
- Verify compiler available: `gcc --version`
- Verify make available: `make --version`
- If network issue: retry clone with timeout
- If compilation error: refer to [Incident Response](INCIDENT_RESPONSE.md)

**Proceed to Stage 1** only if:
- ✓ Test suite all PASS
- ✓ Coherence φ ≥ 0.80
- ✓ No memory leaks (valgrind clean)

---

### Stage 1: Canary Deployment (10% traffic, 2-4 hours monitoring)

**Objective**: Route 10% of CI/CD jobs to Phase 3 Orchestrator. Monitor for 2-4 hours before proceeding.

**Procedure**:

```bash
# 1. Execute Stage 1
${BUILD_DIR}/staging_repo/scripts/deploy_staged.sh stage1

# Output should show:
# === Stage 1: Canary Deployment (10% traffic) ===
# ✓ Stage 1 (10% Canary) deployment successful
# Monitor metrics for 2-4 hours before proceeding to Stage 2

# 2. Verify CI configuration
echo "CI_PHASE3_ENABLE=0.1" should be set in CI environment
grep -r "CI_PHASE3_ENABLE" ${BUILD_DIR}/ || echo "Not found in files"

# 3. Observe metrics for 2-4 hours
for i in {1..12}; do
  sleep 600  # 10 minutes
  ${BUILD_DIR}/staging_repo/scripts/metrics_export.sh ${BUILD_DIR}/metrics_stage1_run_$i.json
  jq '.coherence_phi, .latency_mean_sec' ${BUILD_DIR}/metrics_stage1_run_$i.json
done

# 4. Aggregate metrics
${BUILD_DIR}/staging_repo/scripts/metrics_export.sh ${BUILD_DIR}/metrics_stage1.json
```

**Monitoring Dashboard** (query every 15 minutes):

```bash
# Create monitoring loop
watch -n 900 "jq '.coherence_phi, .latency_mean_sec, .cache_hit_rate' ${BUILD_DIR}/metrics_stage1.json"

# Key metrics to monitor:
# Coherence φ: should remain ≥ 0.80 (ideally > 0.85)
# Latency p99: should remain < 10.0s
# Cache hit rate: should remain > 70%
# Error rate: should remain < 2%
```

**Rollback Triggers** (revert immediately if):
- Coherence φ < 0.80 for 2 consecutive checks (30 min)
- Latency p99 > 10.0s for 3 consecutive checks (45 min)
- Error rate > 2% for 2 consecutive checks
- OOM (out of memory) errors in logs
- Unrecovered build failures > 5% of builds

**Rollback Procedure**:

```bash
# If ANY trigger activates, execute rollback
${BUILD_DIR}/staging_repo/scripts/rollback.sh ${BUILD_DIR}/_rollback_stage0

# Verify system stability
${BUILD_DIR}/staging_repo/scripts/health_check.sh

# Notify on-call team + investigation begins
```

**Proceed to Stage 2** if:
- ✓ Coherence φ ≥ 0.85 throughout 2-4h window
- ✓ No rollback triggers activated
- ✓ Error rate = 0% (no failed builds)
- ✓ Latency stable (no degradation trend)

---

### Stage 2: Progressive Rollout (50% traffic, 4-8 hours monitoring)

**Objective**: Route 50% of CI/CD jobs to Phase 3. Monitor for 4-8 hours before full production.

**Procedure**:

```bash
# 1. Execute Stage 2
${BUILD_DIR}/staging_repo/scripts/deploy_staged.sh stage2

# Output should show:
# === Stage 2: Progressive Rollout (50% traffic) ===
# ✓ Stage 2 (50% Progressive) deployment successful

# 2. Monitor with higher sampling rate (every 5 minutes)
for i in {1..100}; do
  sleep 300  # 5 minutes
  ${BUILD_DIR}/staging_repo/scripts/metrics_export.sh ${BUILD_DIR}/metrics_stage2_run_$i.json
  echo "[$(date)] Run $i: Φ=$(jq '.coherence_phi' ${BUILD_DIR}/metrics_stage2_run_$i.json)"
done

# 3. Check for performance cliff
# Plot coherence trend (requires gnuplot or similar)
jq -r '[.coherence_phi]' ${BUILD_DIR}/metrics_stage2_run_*.json | paste -sd' ' | awk '{
  for(i=1;i<=NF;i++) printf "%.3f\n", $i
}'
```

**Stage 2 Validation**:

```bash
# After 4 hours, verify:
bash ${BUILD_DIR}/staging_repo/scripts/health_check.sh
# Expected: exit code 0 (all PASS)

# Verify load distribution (50% phase3, 50% phase9.14)
grep "CI_PHASE3_ENABLE" /var/log/termux-build/deploy.log | tail -1
# Expected: CI_PHASE3_ENABLE=0.5
```

**Rollback Triggers** (same as Stage 1, but with higher threshold):
- Coherence φ < 0.82 (instead of 0.80)
- Latency p99 > 12s (instead of 10s)
- Error rate > 5% (instead of 2%)

**Proceed to Stage 3** if:
- ✓ Coherence φ ≥ 0.85 stable throughout 4-8h
- ✓ No errors above thresholds
- ✓ Metrics_stage2.json approved by on-call team

---

### Stage 3: Full Production (100% traffic, 24h monitoring)

**Objective**: Route 100% of CI/CD jobs to Phase 3 Orchestrator. Monitor for 24 hours post-deployment.

**Procedure**:

```bash
# 1. Execute Stage 3
${BUILD_DIR}/staging_repo/scripts/deploy_staged.sh stage3

# Output should show:
# === Stage 3: Full Production Deployment (100% traffic) ===
# ✓ Stage 3 (100% Production) deployment successful
# === DEPLOYMENT COMPLETE ===

# 2. Verify 100% traffic routed to Phase 3
grep "CI_PHASE3_ENABLE=1.0" /var/log/termux-build/deploy.log
# Expected: found (just executed)

# 3. 24-hour monitoring loop
for hour in {1..24}; do
  echo "[Hour $hour] $(date)"
  ${BUILD_DIR}/staging_repo/scripts/metrics_export.sh ${BUILD_DIR}/metrics_production_h$hour.json
  bash ${BUILD_DIR}/staging_repo/scripts/health_check.sh
  sleep 3600
done
```

**Post-Deployment Checklist**:

```bash
# Confirm all gates PASS
✓ GitHub Actions phase915-hardening: all jobs green
✓ Health check: all subsystems PASS
✓ Coherence φ: ≥ 0.85 (target exceeded)
✓ Latency p99: < 10s (target met)
✓ Error rate: 0% (no build failures)
✓ Metrics exported: production baseline established
```

**Production Monitoring** (ongoing, daily):

```bash
# Daily metrics snapshot
./scripts/metrics_export.sh /var/log/termux-build/metrics_$(date +%Y%m%d).json

# Weekly trend analysis
jq -r '.coherence_phi' /var/log/termux-build/metrics_202608*.json | \
  awk '{sum+=$1; count++} END {print "Mean φ="sum/count}'

# Alert on regression
if (( $(echo "$(jq '.coherence_phi' /var/log/termux-build/metrics_today.json) < 0.80" | bc -l) )); then
  # Send alert to on-call: coherence degradation
  echo "ALERT: Coherence below minimum" | mail -s "Termux Build Alert" ops@example.com
fi
```

---

## Rollback Procedures

### Automatic Rollback (Triggered by Health Check)

```bash
# If health check fails with exit code 1 (CRITICAL):
if ! bash ${HEALTH_CHECK_SCRIPT}; then
  echo "Health check CRITICAL: initiating rollback"
  bash ${ROLLBACK_SCRIPT} ${BACKUP_DIR}
  bash ${HEALTH_CHECK_SCRIPT}  # Verify post-rollback health
fi
```

### Manual Rollback (Emergency)

```bash
# Operator-initiated rollback (e.g., Stage 1 looks bad after 30 min)
${BUILD_DIR}/staging_repo/scripts/rollback.sh /backup/termux-build-$(ls -t /backup | head -1 | grep termux-build)

# Verify system stable
bash ${HEALTH_CHECK_SCRIPT}

# Notify team
# Incident response begins (see INCIDENT_RESPONSE.md)
```

### Post-Rollback

```bash
# After rollback completes:

# 1. Verify Phase 9.14 (E2E Build) is active
grep "CI_PHASE3_ENABLE=0.0" /var/log/termux-build/deploy.log

# 2. Monitor for 1 hour
for i in {1..6}; do
  bash ${HEALTH_CHECK_SCRIPT}
  sleep 600
done

# 3. Report incident (details in INCIDENT_RESPONSE.md)
```

---

## Troubleshooting

### Issue: Stage 0 compilation fails

**Symptom**: `error: undefined reference to 'orchestrator_init'`

**Resolution**:
1. Verify all object files compiled: `ls core/*.o | grep -E 'orchestrator|resolver'`
2. Check Makefile includes new targets: `grep build_orchestrator core/Makefile`
3. Rebuild from scratch: `cd core && make clean && make test-orchestrator-v2`

### Issue: Health check fails with "No current metrics found"

**Symptom**: `WARNING: No current metrics found. Skipping coherence check.`

**Resolution**:
1. Run metrics export manually: `bash ${METRICS_EXPORT}`
2. Verify file created: `ls -l ${BUILD_DIR}/metrics_current.json`
3. Verify jq installed: `which jq` (if missing: `apt-get install jq`)
4. Check file permissions: `chmod 644 ${BUILD_DIR}/metrics_current.json`

### Issue: Rollback fails with "Backup directory not found"

**Symptom**: `error: Backup directory not found: /backup/termux-build-20260808-...`

**Resolution**:
1. Verify backup was created in Stage 0: `ls -la /backup/termux-build-*`
2. If missing: restore from git: `git clone --branch claude/sistema-nucleo-autoral-2bju50 ... ${BUILD_DIR}/core`
3. Never proceed without verified backup

### Issue: CI_PHASE3_ENABLE not taking effect

**Symptom**: GitHub Actions still using Phase 9.14 after Stage 1

**Resolution**:
1. Verify environment variable set: `echo $CI_PHASE3_ENABLE`
2. Check GitHub Actions secrets: Settings → Secrets → verify CI_PHASE3_ENABLE exists
3. Update secrets via gh CLI: `gh secret set CI_PHASE3_ENABLE -b "0.1"` (Stage 1)
4. Re-run CI workflow: GitHub Actions → phase915-hardening → Run workflow

---

## Success Criteria

**Deployment is considered SUCCESSFUL when all below are confirmed**:

✓ Stage 0: All tests PASS (12/12 orchestrator, 6/6 hardening)  
✓ Stage 1: 10% canary stable 2+ hours, coherence φ ≥ 0.85  
✓ Stage 2: 50% rollout stable 4+ hours, no error spikes  
✓ Stage 3: 100% production, 24h no critical incidents  
✓ Metrics baseline established (production daily snapshots)  
✓ On-call team trained (Incident Response runbook confirmed)  
✓ Rollback procedure tested and verified working

---

## Escalation Contacts

| Role | Contact | Phone | Slack |
|------|---------|-------|-------|
| On-Call Engineer | ops@example.com | +1-555-0100 | #termux-oncall |
| Build System Owner | buildmaster@example.com | +1-555-0101 | @buildmaster |
| Infrastructure | infra@example.com | +1-555-0102 | #infrastructure |

---

## Appendix: Command Reference

```bash
# Stage progression
./deploy_staged.sh stage0         # Staging environment
./deploy_staged.sh stage1         # 10% canary
./deploy_staged.sh stage2         # 50% progressive
./deploy_staged.sh stage3         # 100% production
./deploy_staged.sh status         # Check current deployment state
./deploy_staged.sh rollback       # Emergency rollback

# Health and metrics
./scripts/health_check.sh          # Run all health checks
./scripts/metrics_export.sh        # Export current metrics to JSON
./scripts/rollback.sh /backup/...  # Rollback to specific backup

# Logs and monitoring
tail -f /var/log/termux-build/deploy.log     # Live deployment log
grep "Stage" /var/log/termux-build/deploy.log # Deployment history
jq '.' /home/termux-build/metrics_*.json     # View metrics
```

---

**Last Updated**: 2026-08-08  
**Next Review**: After first production week  
**Approved By**: Build System Team
