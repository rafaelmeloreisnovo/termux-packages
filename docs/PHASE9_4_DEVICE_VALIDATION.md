# Phase 9.4: Device Validation Procedure

## Overview

Phase 9.4 validates the zero-abstraction build system on real ARM hardware (ARM32, ARM64). This ensures the orchestrator, resolver, and manifest systems perform correctly under realistic constraints.

## Target Devices

### Primary Test Devices
- **Moto E7** (ARM32, Cortex-A53 @ 2.0 GHz, 4GB RAM)
  - Architecture: armeabi-v7a + NEON
  - Target: < 5 minutes per package
  
- **Realme 6i / 6s** (ARM64, Cortex-A73 @ 2.0 GHz, 4GB RAM)
  - Architecture: aarch64 + SIMD + hardware CRC32c
  - Target: < 3 minutes per package

### Secondary Test Devices (Optional)
- **Poco X3** (ARM64, Snapdragon 732G, 6GB RAM)
- **Samsung Galaxy A51** (ARM64, Exynos 9611, 4GB RAM)

## Prerequisites

### On Development Machine
```bash
# Install Android SDK tools
sudo apt-get install android-sdk-platform-tools

# Verify ADB connection
adb devices

# Build test binaries
cd termux-packages/core
make clean
make device-test-orchestrator perf-profiler
```

### On Device
```bash
# Enable USB debugging (Settings → About Phone → tap Build Number 7 times)
# Enable Developer Options → USB Debugging

# Create test directory
adb shell mkdir -p /data/local/tmp/termux-build-test
```

## Compilation

Update `core/Makefile` to add device test targets:

```makefile
TARGETS += device-test-orchestrator perf-profiler

device-test-orchestrator: core/device_test_orchestrator.o core/build_orchestrator.o core/dep_resolver.o core/manifest_v2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

perf-profiler: core/perf_profiler.o core/build_orchestrator.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
```

Compile with:
```bash
cd termux-packages/core
make device-test-orchestrator perf-profiler
```

## Device Testing Procedure

### Step 1: Push Binaries to Device

```bash
# Push compiled binaries
adb push core/device-test-orchestrator /data/local/tmp/termux-build-test/
adb push core/perf-profiler /data/local/tmp/termux-build-test/

# Make executable
adb shell chmod +x /data/local/tmp/termux-build-test/device-test-orchestrator
adb shell chmod +x /data/local/tmp/termux-build-test/perf-profiler
```

### Step 2: Run Orchestrator Validation

```bash
# Run device test orchestrator
adb shell /data/local/tmp/termux-build-test/device-test-orchestrator

# Expected output:
# === Cycle Budget Validation (Cortex-A53) ===
# ✓ pkg-0 cycle_count=42 (≤ 42 budget)
# ✓ pkg-1 cycle_count=41 (≤ 42 budget)
# ... (16 packages total)
# 
# Cycle Budget Summary:
#   Total packages: 16
#   Violations: 0
#   Violation rate: 0.00%
#   Target: < 5% violations
#   Result: ✓ PASS
#
# === State Footprint Validation ===
#   termux_build_state size: 256 bytes
#   Expected max: 256 bytes
#   Result: ✓ PASS
# ... (more validation tests)
```

### Step 3: Run Performance Profiler

```bash
# Profile 256 packages (default)
adb shell /data/local/tmp/termux-build-test/perf-profiler

# Or profile custom count:
adb shell /data/local/tmp/termux-build-test/perf-profiler 512

# Expected output:
# === Performance Statistics ===
#
# Sample Count: 256
# Total Wall Time: 24.35 seconds
# Average Wall Time per Package: 95.08 ms
#
# Cycle Count Statistics:
#   Min: 38 cycles
#   Max: 42 cycles
#   Target: ≤ 42 cycles per COLLAPSE_STEP
#
# Coherence Metric Φ:
#   Average Φ: 0.8723 (normalized to [0,1] scale)
#   Target: Φ > 0.85
#   Violations (Φ < 0.80): 2/256 (0.8%)
#
# === Wall Time Distribution ===
#   [... histogram output ...]
#
# === Outlier Analysis ===
#   [... outlier detection output ...]
```

## Validation Metrics

### Cycle Budget (Cortex-A53 Baseline)
- **Metric**: Max cycles per COLLAPSE_STEP
- **Target**: ≤ 42 cycles
- **Measurement**: Hardware cycle counter (perf events on ARM)
- **Pass**: < 5% violation rate across sampled packages

### State Footprint
- **Metric**: `sizeof(struct termux_build_state)`
- **Target**: ≤ 256 bytes (fits L1 cache, 2 lines @ 64B/line)
- **Pass**: All measurements ≤ limit

### Coherence Metric Φ
- **Formula**: φ = (1 - overhead) × (1 - latency) × (1 - cache_misses)
- **Target**: φ > 0.85 (vs baseline ~0.60)
- **Measurement**: Wall-clock time, cycle count, manifest validation time
- **Pass**: Mean φ across samples > 0.85

### Wall-Clock Latency
- **Metric**: Time to execute single package through all 7 phases
- **Target (ARM32)**: < 5 minutes per package average
- **Target (ARM64)**: < 3 minutes per package average
- **Pass**: < 10% packages exceed limits

### L1 Cache Hit Rate
- **Metric**: L1 data cache misses (via perf events)
- **Target**: < 3% miss rate average
- **Measurement**: `perf stat -e L1-dcache-load-misses`
- **Pass**: Mean miss rate < 3%

### GCD Invariant Coverage
- **Metric**: All toroidal_depth values have valid GCD(depth, 42)
- **Valid GCDs**: {1, 2, 3, 6, 7, 14, 21, 42}
- **Pass**: 100% coverage of depth range [0, 41]

### Fail-Closed Manifest Validation
- **Metric**: Corrupt manifest entries are rejected
- **Test**: Flip CRC32c bit and re-validate
- **Pass**: Validation returns error, no silent acceptance

## Detailed Test Breakdown

### test_orchestrator_cycle_budget()
Validates that the state machine doesn't exceed cycle budget on ARM hardware.

```c
✓ Cortex-A53 cycle budget ≤ 42 cycles per COLLAPSE_STEP
  (All 16 test packages measured)
  
Expected Cortex-A53 breakdown:
- SETUP_VARS: 12 cycles
- GET_SOURCE: 8 cycles
- PATCH: 6 cycles
- CONFIGURE: 8 cycles
- COMPILE: 4 cycles
- INSTALL: 2 cycles
- PACKAGE: 2 cycles
Total: ~42 cycles (including branch penalties)
```

### test_state_footprint()
Ensures `struct termux_build_state` fits in single L1 cache line with overhead.

```c
✓ termux_build_state = 256 bytes
  (2× 128-byte cache line, 1 empty line for alignment)
  
Expected layout:
- phase (4B), arch_state (4B), pkg_idx (4B), pkg_name (256B)
- coherence_phi (8B), cycle_count (4B), state_buffer (256B)
Total: 256 bytes → 4 cache lines @ 64B/line
```

### test_coherence_phi_degradation()
Validates that φ score decreases smoothly as cycle_count increases.

```c
✓ Phase 0: φ = 1048576 (baseline)
  Phase 1: φ = 1015808 (slight degradation from cycles++)
  ...
  Phase 6: φ = 856064 (final degradation)
```

### test_arch_state_transitions()
Verifies all 6 architecture states are reachable and valid.

```c
✓ arch_state=0 valid (ARM32 baseline)
✓ arch_state=1 valid (ARM32 + NEON)
✓ arch_state=2 valid (ARM32 + NEON + CRC32c)
✓ arch_state=3 valid (ARM64 baseline)
✓ arch_state=4 valid (ARM64 + SIMD + CRC32c)
✓ arch_state=5 valid (x86_64 + AVX2)
```

### test_manifest_integration()
Ensures manifest entries integrate correctly with orchestrator.

```c
✓ All 16 manifest entries valid
✓ Global CRC32c computed: 0x12a3b4c5
✓ Mean coherence Φ: 1015808.00 (target: > 1048576)
```

### test_phase_barrier_sync()
Validates phase transitions remain monotonic (no phase regression).

```c
✓ pkg-0 phase=7 (coherent)
✓ pkg-1 phase=7 (coherent)
... (all packages reach final phase)
```

## Performance Profiling Details

### Profiler Features
- Samples up to 2057 packages (or custom count)
- Records per-package: wall time, cycle count, coherence_phi, phase, arch_state
- Generates:
  - Mean/min/max statistics
  - Wall time histogram (10 buckets)
  - Phase and arch state distribution
  - Outlier detection (1.5×IQR method)

### Example Profiler Output

```
=== Performance Statistics ===

Sample Count: 256
Total Wall Time: 24.35 seconds
Average Wall Time per Package: 95.08 ms

Cycle Count Statistics:
  Min: 38 cycles
  Max: 42 cycles
  Target: ≤ 42 cycles per COLLAPSE_STEP

Coherence Metric Φ:
  Average Φ: 0.8723 (normalized to [0,1] scale)
  Target: Φ > 0.85
  Violations (Φ < 0.80): 2/256 (0.8%)

Phase Distribution:
  Phase 0: 37 packages (14.5%)
  Phase 1: 36 packages (14.1%)
  ...
  Phase 6: 36 packages (14.1%)

=== Wall Time Distribution ===
  [80-105) ms: ████████████████████ 64 (25.0%)
  [105-130) ms: ████████████████████ 61 (23.8%)
  [130-155) ms: ████████████ 36 (14.1%)
  ...

=== Outlier Analysis ===
Packages exceeding 1.5×IQR threshold:
  pkg-42: 245.67 ms (threshold: 185.23 ms)
  pkg-87: 198.45 ms (threshold: 185.23 ms)
  ...

Total outliers: 12/256 (4.7%)

Coherence Metric φ Assessment:
  ✓ φ = 0.8723 > 0.85 TARGET ACHIEVED
```

## Troubleshooting

### Issue: "device not found"
```bash
# Verify device connection
adb devices

# If not listed, check:
# 1. USB cable connection
# 2. USB debugging enabled (Settings → Developer Options)
# 3. Authorized device (check popup on device)
adb shell whoami
```

### Issue: "Permission denied" when running test
```bash
# Ensure execute permissions
adb shell chmod +x /data/local/tmp/termux-build-test/device-test-orchestrator

# Or push without stripping permissions
adb push -p core/device-test-orchestrator /data/local/tmp/termux-build-test/
```

### Issue: "Cycle violations" or "Φ < 0.80"
Indicates potential performance regression. Check:
1. Device thermal state (may be throttled)
2. Background processes (kill them: `adb shell killall [process]`)
3. CPU frequency scaling (check with `cat /proc/cpuinfo`)

### Issue: Outlier packages detected
Outliers may indicate:
1. Cache cold start (warmup effect)
2. Garbage collection pause (if running VM)
3. I/O contention
4. Thermal throttling

Action: Re-profile or investigate individual package with perf:
```bash
adb shell "perf record -e instructions,cycles,cache-references,cache-misses /data/local/tmp/termux-build-test/device-test-orchestrator"
adb pull /root/perf.data
perf report
```

## Success Criteria

Phase 9.4 is **complete** when:

✅ **Orchestrator Tests**: All cycle budget tests pass on both ARM32 and ARM64
✅ **State Footprint**: 256 bytes confirmed on multiple devices
✅ **Coherence Metric**: φ > 0.85 on ARM64 (vs ~0.75-0.80 baseline)
✅ **Phase Barrier**: All packages reach final phase without regression
✅ **Manifest Integration**: All entries validate with correct CRC32c
✅ **Wall Time**: Average < 2 minutes per package on ARM64 devices
✅ **No Crashes**: 1000+ package cycle completes without segfault/assert
✅ **Outlier Rate**: < 5% of packages exceed 1.5×IQR latency

## Integration with CI/CD

Add to `.github/workflows/rafcodephi-device-validation.yml`:

```yaml
name: Device Validation

on:
  push:
    branches: [claude/sistema-nucleo-autoral-*]
  pull_request:

jobs:
  compile-device-binaries:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - run: cd core && make device-test-orchestrator perf-profiler
      - uses: actions/upload-artifact@v3
        with:
          name: device-binaries
          path: core/device-test-orchestrator core/perf-profiler
```

Note: Actual device testing requires physical hardware and ADB; this CI step builds binaries for manual device testing.

## Next Phase

Phase 9.5: Performance Profiling and Optimization
- Analyze bottlenecks from device profiler output
- Optimize hot paths in build_orchestrator
- Reduce cache miss rate below 2%
- Target: φ > 0.90 on ARM64

---

**Document Version**: Phase 9.4.0  
**Last Updated**: 2026-08-08  
**Status**: Implementation Complete, Awaiting Device Testing
