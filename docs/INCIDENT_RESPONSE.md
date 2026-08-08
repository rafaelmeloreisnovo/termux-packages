# Phase 9.18: Incident Response Procedures

**Version**: 1.0  
**Date**: 2026-08-08  
**Branch**: `claude/sistema-nucleo-autoral-2bju50`  
**Audience**: On-Call Engineers, Build System Owners

---

## Quick Reference: Emergency Actions

| Incident | Immediate Action | Time | Escalate |
|----------|------------------|------|----------|
| Build timeout (> 30min) | `./deploy_staged.sh rollback` | 5 min | If manual fix fails |
| OOM (out of memory) | Rollback Stage, kill `termux-build-core` | 5 min | Infrastructure team |
| Coherence φ crash (< 0.75) | Auto-rollback triggered | 2 min | Investigate cause |
| CI pipeline failure | Check GitHub Actions logs | 10 min | Build system owner |
| Disk space exhausted | Clear `_build_cache`, `_checkpoints` | 5 min | Resize storage |
| Network connectivity loss | Pause deployment, investigate | 10 min | Infra + network team |

---

## Incident Categories & Response

### Category 1: Coherence Metric Degradation (φ < 0.80)

**Symptoms**:
- Health check reports `CRITICAL: Coherence φ < minimum 0.80`
- Metrics show φ trending downward over 30 minutes
- CI/CD latency increasing on Phase 3 builds

**Root Cause Analysis**:

```bash
# 1. Check which components degraded
jq '.[] | {coherence_phi, latency_mean_sec, cache_hit_rate, overhead_heap}' \
  /home/termux-build/metrics_stage*.json | head -20

# Expected component degradation indicators:
# - If coherence_phi↓ + latency_mean_sec↑ → latency penalty (COLLAPSE_PHASE overhead?)
# - If coherence_phi↓ + cache_hit_rate↓ → L1D misses (memory alignment issue?)
# - If coherence_phi↓ + overhead_heap↑ → malloc churn (checkpoint serialization?)

# 2. Check Phase 3 Orchestrator logs
grep "error\|CRITICAL\|panic" /var/log/termux-build/deploy.log | tail -20

# 3. Check specific package causing slowdown
cd ${BUILD_DIR}/staging/core
./test-orchestrator-v2 2>&1 | grep -E "Phase|arch_state|coherence"

# 4. Profiling (if available)
perf record -e cycles,cache-references,cache-misses ./test-orchestrator-v2
perf report
```

**Emergency Response** (< 5 minutes):

```bash
# Step 1: Automatic detection + rollback
if (( $(echo "$(jq '.coherence_phi' /home/termux-build/metrics_current.json) < 0.80" | bc -l) )); then
  ${BUILD_DIR}/staging_repo/scripts/rollback.sh /backup/termux-build-$(date -d '1 hour ago' +%Y%m%d)
fi

# Step 2: Verify Phase 9.14 (E2E Build) still working
cd ${BUILD_DIR}
make test-e2e-build
./test-e2e-build
# Expected: all tests PASS

# Step 3: Notify on-call team
echo "INCIDENT: Coherence degradation detected, rolled back to Phase 9.14" | \
  mail -s "Termux Build Alert" ops@example.com
```

**Investigation** (post-rollback, 30+ min):

```bash
# 1. Compare metrics before vs after rollback
echo "=== Pre-Rollback (Stage X) ==="
jq '.coherence_phi, .latency_mean_sec' /home/termux-build/metrics_stageX.json

echo "=== Post-Rollback (Current) ==="
${BUILD_DIR}/staging_repo/scripts/metrics_export.sh
jq '.coherence_phi, .latency_mean_sec' /home/termux-build/metrics_current.json

# 2. Examine orchestrator state machine (assembly level)
objdump -d ${BUILD_DIR}/staging/core/termux-build-core | grep -A 50 "COLLAPSE_PHASE"

# 3. Check for memory alignment issues
cd ${BUILD_DIR}/staging/core
valgrind --tool=helgrind ./test-orchestrator-v2 2>&1 | head -50

# 4. Run isolated dependency resolver test
./test-dep-resolver 2>&1 | grep -E "layer|coherence|depth"

# 5. Check git diff for unintended changes
cd ${BUILD_DIR}/staging_repo
git diff HEAD^..HEAD -- core/build_orchestrator_v2.c | head -100
```

**Root Cause Examples & Fixes**:

| Cause | Evidence | Fix |
|-------|----------|-----|
| COLLAPSE_PHASE overhead | latency_mean_sec ↑30%, cache_hit_rate stable | Optimize assembly: fewer branches, better register usage |
| Memory alignment | cache_hit_rate ↓15%, heap same | Align build_state_t to 256B cache line boundary |
| malloc churn | overhead_heap ↑ to 8%, latency stable | Verify pre-allocated buffers, check checkpoint serialization |
| Dependency resolution | layer batching incorrect, depth misassignment | Verify topological sort correctness in dep_resolver_v2.c |

---

### Category 2: Build Timeout (Exceeds 5 minutes per package)

**Symptoms**:
- CI/CD workflow timeout after 30+ minutes on Stage 1/2
- Individual package build > 5 minutes (baseline 2-3s)
- "timeout: job execution timed out" in GitHub Actions logs

**Root Cause Analysis**:

```bash
# 1. Check which package is hanging
ps aux | grep -E "make|cc|ld|build" | grep -v grep

# 2. Examine build log for last successful phase
tail -100 /var/log/termux-build/deploy.log | grep -E "Phase|package|state"

# 3. Check system resource exhaustion
df -h                    # Disk space
free -h                  # Memory
top -bn1 | head -20      # CPU usage

# 4. Trace the hanging build
if pidof termux-build-core; then
  pid=$(pgrep -f termux-build-core | head -1)
  strace -p $pid -e open,read,write 2>&1 | head -50
fi
```

**Emergency Response** (< 5 minutes):

```bash
# Step 1: Kill hanging build
pkill -9 termux-build-core
pkill -9 build-package.sh
sleep 2

# Step 2: Check if recovery possible
if [ -d "${BUILD_DIR}/_checkpoints" ]; then
  echo "Checkpoint available, resume may work"
  # Resume from checkpoint (if supported)
  ${BUILD_DIR}/staging_repo/scripts/deploy_staged.sh resume
fi

# Step 3: If resume fails, rollback
if [ $? -ne 0 ]; then
  ${BUILD_DIR}/staging_repo/scripts/rollback.sh /backup/termux-build-latest
fi
```

**Investigation** (post-recovery, 30+ min):

```bash
# 1. Check for I/O bottleneck
iotop -b -n 5

# 2. Check for resource leak
# If heap grows during build, may indicate memory leak in Phase 3
valgrind --leak-check=full --log-file=valgrind.log \
  ${BUILD_DIR}/staging/core/termux-build-core

# 3. Profile specific phase
time ${BUILD_DIR}/staging/core/termux-build-core --phase=4 --package=busybox
# If COMPILE phase (4) takes > 60s, investigate gcc flags

# 4. Check disk I/O for checkpoint serialization
iostat -x 1 5 > io_profile.txt
```

**Common Causes & Fixes**:

| Cause | Fix |
|-------|-----|
| Disk exhaustion (cache full) | `rm -rf ${BUILD_DIR}/_build_cache/*` |
| Infinite loop in orchestrator | Add phase timeout: `PHASE_TIMEOUT=120s` |
| Network issue (source download) | Retry: `--retry-count=3` |
| Compiler cache corrupted | `gcc --clear-cache` or rebuild gcc |

---

### Category 3: Memory Exhaustion (OOM Killer)

**Symptoms**:
- `dmesg` shows `OOM-Killer: Killed process XXX`
- Build abruptly stops with no error message
- Swap usage spikes to disk limit

**Emergency Response** (< 2 minutes):

```bash
# Step 1: Kill hanging processes
pkill -9 termux-build-core

# Step 2: Clear build cache to free memory
rm -rf ${BUILD_DIR}/_build_cache/*
rm -rf ${BUILD_DIR}/_checkpoints/*

# Step 3: Check available memory
free -h
# If < 500MB available, need external intervention

# Step 4: Rollback if still low on memory
if [ $(free -b | awk 'NR==2{print $7}') -lt 500000000 ]; then
  ${BUILD_DIR}/staging_repo/scripts/rollback.sh /backup/termux-build-latest
fi
```

**Investigation**:

```bash
# 1. Analyze memory profile
dmesg | tail -20 | grep -i "oom\|killed"

# 2. Check for memory leak in Phase 3
valgrind --tool=massif ${BUILD_DIR}/staging/core/termux-build-core
cat massif.out.* | grep -E "mem_heap_B|total_allocd"

# 3. Verify build_state_t and dep_graph_t sizes
echo "build_state_t size: $(grep 'sizeof(build_state_t)' /var/log/termux-build/deploy.log)"
echo "Expected: 256 bytes"

# 4. Check for unbounded buffer in checkpoint serialization
grep -n "malloc\|calloc" ${BUILD_DIR}/staging_repo/core/production_hardening.c
```

**Root Causes & Fixes**:

| Cause | Evidence | Fix |
|-------|----------|-----|
| Memory leak in orchestrator | massif shows mem ↑ over time | Check COLLAPSE_PHASE for unfreed allocations |
| Unbounded buffer in resolver | deps_col array too large | Verify CSR format allocation: ~14K edges max |
| Checkpoint size ballooning | _checkpoints/ dir > 1GB | Truncate old checkpoints: `find ... -mtime +1 -delete` |

---

### Category 4: CI Pipeline Failure (GitHub Actions)

**Symptoms**:
- GitHub Actions workflow shows ✗ (red) on phase915-hardening
- "Actions" tab shows job failure: "The operation was canceled"
- No error output in logs

**Emergency Response** (< 5 minutes):

```bash
# Step 1: Check GitHub Actions dashboard
# https://github.com/rafaelmeloreisnovo/termux-packages/actions

# Step 2: View detailed job logs
# Actions → phase915-hardening → Latest run → click failed job
# Look for error lines (usually at end of output)

# Step 3: If timeout error, increase timeout and re-run
# Edit .github/workflows/phase915-hardening.yml:
# timeout-minutes: 120  # increase from default 60

# Step 4: Re-run failed job via GitHub UI
# Actions → Failed job → "Re-run jobs"
```

**Common CI Failures & Fixes**:

| Failure | Fix |
|---------|-----|
| `error: undefined reference to orchestrator_init` | Rebuild artifacts: clear cache in Actions settings |
| `Timeout after 60 minutes` | Increase timeout-minutes in workflow YAML |
| `No space left on device` | Clear Actions cache: Settings → Actions → clear all |
| `Connection refused` (git clone) | Network issue, retry with exponential backoff |

---

### Category 5: Cascading Failures (Multiple Subsystems)

**Symptoms**:
- Multiple incident types occurring simultaneously
- CI pipeline + health check + manual tests all failing
- Loss of visibility into system state

**Emergency Response** (Immediate):

```bash
# PRIORITY 1: Stabilize system
pkill -9 termux-build-core termux-packages build-package.sh

# PRIORITY 2: Full rollback
${BUILD_DIR}/staging_repo/scripts/rollback.sh /backup/termux-build-$(date +%Y%m%d)

# PRIORITY 3: Verify Phase 9.14 functional
cd ${BUILD_DIR}/core
make clean && make test-e2e-build
./test-e2e-build && echo "✓ Phase 9.14 recovered"

# PRIORITY 4: Notify team
echo "CRITICAL: Cascading failure during Stage X, rolled back to Phase 9.14" | \
  mail -s "URGENT: Termux Build Incident" ops@example.com oncall@example.com
```

**Post-Incident Investigation** (1-2 hours):

```bash
# 1. Capture full system state
uname -a > incident_report.txt
df -h >> incident_report.txt
free -h >> incident_report.txt
dmesg | tail -50 >> incident_report.txt

# 2. Analyze logs chronologically
cat /var/log/termux-build/deploy.log | grep -E "ERROR|CRITICAL|panic" | head -20

# 3. Review git history
cd ${BUILD_DIR}/staging_repo
git log --oneline -20

# 4. Compare phase915-hardening CI output
# https://github.com/rafaelmeloreisnovo/termux-packages/actions
# Download latest run artifacts + logs

# 5. Escalate to Build System Owner for deep investigation
```

---

## Recovery Procedures

### Scenario: Rollback Succeeded, Resume Deployment

**Procedure**:

```bash
# 1. Verify current state (should be Phase 9.14)
echo $CI_PHASE3_ENABLE
# Expected: 0.0

# 2. Investigation period (2-4 hours minimum)
# - Root cause analysis complete
# - Fix implemented and tested in staging
# - New build artifacts generated

# 3. Restart deployment from Stage 0
${BUILD_DIR}/staging_repo/scripts/deploy_staged.sh stage0
# Re-run full test suite, metrics collection
```

### Scenario: Rollback Failed (Emergency Manual Recovery)

**Procedure**:

```bash
# Step 1: Manual Phase 9.14 deployment
cd /home/termux-build
git clone https://github.com/rafaelmeloreisnovo/termux-packages.git core_recovery
cd core_recovery
git checkout main  # Switch to Phase 9.14 (main branch)

# Step 2: Verify compilation
make clean test-e2e-build
./test-e2e-build

# Step 3: If successful, replace production
rm -rf /home/termux-build/core
mv /home/termux-build/core_recovery /home/termux-build/core

# Step 4: Update CI environment
export CI_PHASE3_ENABLE=0.0

# Step 5: Verify system stable
bash /home/termux-build/scripts/health_check.sh
```

### Scenario: Data Corruption Detected

**Procedure**:

```bash
# 1. Identify corrupted component
# - Manifest checksum mismatch: rebuild manifest
# - Checkpoint corruption: discard _checkpoints/
# - Cache corruption: clear _build_cache/

# 2. Isolate and fix
if ! jq empty ${BUILD_DIR}/metrics_*.json 2>/dev/null; then
  echo "Corrupted JSON metrics"
  rm -f ${BUILD_DIR}/metrics_*.json
fi

# 3. Rebuild affected data
${BUILD_DIR}/staging_repo/scripts/metrics_export.sh ${BUILD_DIR}/metrics_current.json

# 4. Verify integrity
jq '.' ${BUILD_DIR}/metrics_current.json > /dev/null
```

---

## Post-Incident Review

**Within 1 hour of incident resolution**:

1. **Notify team**: Send incident summary to ops + build system owner
2. **Document timeline**: Log exact timestamps of events
3. **Preserve artifacts**: Keep logs, metrics, core dumps for analysis

**Within 24 hours**:

1. **Root cause analysis** (RCA): What failed and why?
2. **Corrective action plan**: How to prevent recurrence?
3. **Test coverage**: Add regression tests for this failure mode

**Example Post-Incident Report**:

```markdown
# Incident Report: Coherence φ Degradation (Stage 1)

**Date**: 2026-08-09 14:30 UTC  
**Duration**: 12 minutes  
**Severity**: Critical (rolled back)  
**Impact**: 10% of CI builds affected

## Timeline
- 14:30: Health check detected φ=0.78 (below 0.80 minimum)
- 14:32: Automatic rollback triggered
- 14:35: Phase 9.14 confirmed operational
- 14:45: Root cause identified

## Root Cause
Memory alignment issue in build_state_t structure:
- Expected L1 cache line: 64 bytes
- Actual alignment: 32 bytes (WRONG)
- Result: False sharing between threads → cache coherency overhead

## Fix
```c
// Before
typedef struct { ... } build_state_t;  // 256 bytes, unaligned

// After
typedef struct { ... } __attribute__((aligned(64))) build_state_t;  // 256 bytes, cache-line aligned
```

## Prevention
- Add alignment validation to CI: `assert(sizeof(build_state_t) % 64 == 0)`
- Add false-sharing detector test: run orchestrator on multi-core, verify φ > 0.85
```

---

## Escalation Matrix

| Severity | Detection | Action | Escalate To | SLA |
|----------|-----------|--------|-------------|-----|
| **Critical** | φ < 0.80 or timeout | Auto-rollback | On-call + Build Owner | 5 min |
| **Warning** | 0.80 ≤ φ < 0.85 | Monitor + investigate | On-call | 30 min |
| **Info** | φ ≥ 0.85 | Log event | Build system team | None |

**On-Call Rotation**: ops@example.com (primary), buildmaster@example.com (secondary)

---

## Checklists

### Incident Declaration Checklist
- [ ] Severity level determined
- [ ] Incident #123 created (Jira/GitHub Issues)
- [ ] Slack channel #termux-incident created
- [ ] On-call notified (Slack + email)
- [ ] Rollback decision made (auto or manual)

### Post-Rollback Verification
- [ ] Phase 9.14 test-e2e-build PASSES
- [ ] Health check ALL PASS
- [ ] CI_PHASE3_ENABLE = 0.0 confirmed
- [ ] Production builds succeeding (sampling 10 builds)
- [ ] No residual errors in logs

### RCA Completion
- [ ] Root cause identified + documented
- [ ] Fix implemented + tested
- [ ] Regression test added to CI
- [ ] DEPLOYMENT_RUNBOOK.md updated if needed
- [ ] Team training scheduled (if systemic)

---

**Document Version**: 1.0  
**Last Updated**: 2026-08-08  
**Approved By**: Build System Team  
**Next Review**: After first production month
