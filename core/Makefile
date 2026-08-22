CC ?= cc
CFLAGS ?= -Wall -Wextra -Wshadow -Werror -g -O0
LDFLAGS ?=

# REAL-friction preset — reduces friction/symbols/dead code in REAL binaries.
# Warnings feed the linker: unused symbols → gc'd; hidden by default so only
# what's actually used gets exported. -flto lets the linker cross TU boundaries.
REAL_CFLAGS  := -Wall -Wextra -Wshadow -Werror -Wunused -Wunreachable-code \
                -O2 -g \
                -fdata-sections -ffunction-sections \
                -fvisibility=hidden \
                -flto
REAL_LDFLAGS := -Wl,--gc-sections -flto

# REAL provenance injection — every REAL binary knows the exact tree,
# time, and toolchain it was built from.
REAL_GIT_COMMIT       := $(shell git rev-parse HEAD 2>/dev/null || echo TOKEN_VAZIO_no_git)
REAL_BUILD_TIMESTAMP  := $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
REAL_CFLAGS_FP        := $(shell printf '%s' '$(REAL_CFLAGS)' | sha256sum | cut -c1-16)
REAL_PROV_DEFS := \
  -DREAL_GIT_COMMIT=\"$(REAL_GIT_COMMIT)\" \
  -DREAL_BUILD_TIMESTAMP=\"$(REAL_BUILD_TIMESTAMP)\" \
  -DREAL_CFLAGS_FP=\"$(REAL_CFLAGS_FP)\"

TARGETS := termux-build-core test-manifest manifest-dumper test-cycle2-1 test-cycle2-2 test-cycle2-3 test-cycle2-4 test-build-orchestrator test-dep-resolver test-manifest-v2 device-test-orchestrator perf-profiler perf-analyzer simd-benchmark multicore-benchmark advanced-simd-benchmark advanced-simd-test orchestrator-production-benchmark unified-benchmark test-sprint3 test-sprint4 test-sprint5 test-sprint6 test-sprint10 test-sprint11 test-sprint12 orchestrator-build-main test-bitraf64-integration test-bitraf64-simd-ops test-bitraf64-lut-cache test-bitraf64-coherence-monitor test-ascii-grafo test-e2e-build test-production-hardening test-orchestrator-v2 test-performance-opt test-distributed test-security-hardening test-integration-e2e pkg-real metrics-producer pkg-count-freestanding contract-validate arch-detect receipt-validate receipt-ledger arch-probe
SOURCES := termux-build-core.c sysexec.c package_writer.c elf_writer.c friction_analyzer.c checkpoint.c parallel_jobs.c build_exec.c manifest_loader.c source_download.c patch_apply.c build_orchestrator.c dep_resolver.c manifest_v2.c device_test_orchestrator.c perf_profiler.c build_orchestrator_optimized.c perf_analyzer.c build_orchestrator_simd.c phase_barrier_lockfree.c cpu_affinity.c job_scheduler_parallel.c hardware_tuning.c build_orchestrator_advanced_simd.c unified_orchestrator.c gpu_device.c cycle_budget.c workload_generator.c coherence_tuning.c profiler.c multicore_orchestrator.c advanced_vectorization.c work_stealing.c adaptive_dvfs.c gpu_integration.c orchestrator_build_main.c bitraf64_integration.c bitraf64_simd_ops.c bitraf64_lut_cache.c bitraf64_coherence_monitor.c ascii_grafo.c ascii_grafo_integration.c production_hardening.c build_orchestrator_v2.c dep_resolver_v2.c manifest_simd.c scheduler_adaptive.c alloc_pool.c dist_orchestrator.c rpc_coordinator.c dist_checkpoint.c load_balancer.c crypto_ed25519.c crypto_chacha20.c rate_limiter.c crypto_pqc.c
TEST_SOURCES := test_manifest.c manifest_loader.c
DUMPER_SOURCES := manifest_dumper.c
OBJECTS := $(SOURCES:.c=.o)
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)
DUMPER_OBJECTS := $(DUMPER_SOURCES:.c=.o)

.PHONY: all clean test manifest real-all real-tour real-test real-hygiene cycle2-source-gate cycle2-extract-gate cycle2-patch-gate cycle2-manifest-gate cycle2-dep-gate

all: $(TARGETS)

termux-build-core: termux-build-core.o sysexec.o package_writer.o elf_writer.o friction_analyzer.o checkpoint.o parallel_jobs.o build_exec.o manifest_loader.o source_download.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-manifest: test_manifest.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

manifest-dumper: manifest_dumper.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-cycle2-1: tests/test_cycle2_1_manifesto_v2.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-cycle2-2: tests/test_cycle2_2_source_download.o manifest_loader.o source_download.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-cycle2-3: tests/test_cycle2_3_source_patch.o patch_apply.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lssl -lcrypto

test-cycle2-4: tests/test_cycle2_4_manifest_v2.o manifest_v2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-build-orchestrator: tests/test_build_orchestrator.o build_orchestrator.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-dep-resolver: tests/test_dep_resolver.o dep_resolver.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-manifest-v2: tests/test_manifest_v2.o manifest_v2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

device-test-orchestrator: device_test_orchestrator.o build_orchestrator.o manifest_v2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

perf-profiler: perf_profiler.o build_orchestrator.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

perf-analyzer: perf_analyzer.o build_orchestrator.o build_orchestrator_optimized.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

simd-benchmark: simd_benchmark.o build_orchestrator.o build_orchestrator_simd.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

multicore-benchmark: multicore_benchmark.o build_orchestrator.o phase_barrier_lockfree.o cpu_affinity.o job_scheduler_parallel.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lpthread -lm

advanced-simd-benchmark: simd_benchmark.o build_orchestrator.o build_orchestrator_advanced_simd.o hardware_tuning.o cpu_affinity.o
	$(CC) $(CFLAGS) -mavx512f -o $@ $^ $(LDFLAGS) -lm

advanced-simd-test: tests/test_advanced_simd.o build_orchestrator_advanced_simd.o hardware_tuning.o cpu_affinity.o
	$(CC) $(CFLAGS) -mavx512f -o $@ $^ $(LDFLAGS) -lm

orchestrator-production-benchmark: orchestrator_production_benchmark.o build_orchestrator.o phase_barrier_lockfree.o cpu_affinity.o job_scheduler_parallel.o hardware_tuning.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lpthread -lm

unified-benchmark: unified_benchmark.o unified_orchestrator.o gpu_device.o phase_barrier_lockfree.o hardware_tuning.o cpu_affinity.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lpthread -lm

orchestrator-build-main: orchestrator_build_main.o unified_orchestrator.o workload_generator.o build_orchestrator.o dep_resolver.o phase_barrier_lockfree.o hardware_tuning.o cpu_affinity.o gpu_device.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lpthread -lm

test-bitraf64-integration: tests/test_bitraf64_integration.o bitraf64_integration.o manifest_v2.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-bitraf64-simd-ops: tests/test_bitraf64_simd_ops.o bitraf64_simd_ops.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-bitraf64-lut-cache: tests/test_bitraf64_lut_cache.o bitraf64_lut_cache.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-bitraf64-coherence-monitor: tests/test_bitraf64_coherence_monitor.o bitraf64_coherence_monitor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-ascii-grafo: tests/test_ascii_grafo.o ascii_grafo.o ascii_grafo_integration.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-e2e-build: tests/test_e2e_build.o workload_generator.o dep_resolver.o ascii_grafo.o ascii_grafo_integration.o bitraf64_coherence_monitor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-production-hardening: tests/test_production_hardening.o production_hardening.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-orchestrator-v2: tests/test_orchestrator_v2.o build_orchestrator_v2.o dep_resolver_v2.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-performance-opt: tests/test_performance_opt.o manifest_simd.o scheduler_adaptive.o alloc_pool.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-distributed: tests/test_distributed.o dist_orchestrator.o rpc_coordinator.o dist_checkpoint.o load_balancer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-security-hardening: tests/test_security_hardening.o crypto_ed25519.o crypto_chacha20.o rate_limiter.o crypto_pqc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

test-integration-e2e: tests/test_integration_e2e.o dist_orchestrator.o rpc_coordinator.o dist_checkpoint.o load_balancer.o crypto_ed25519.o crypto_chacha20.o rate_limiter.o crypto_pqc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

# REAL: inventory + parser + DAG (no simulation)
pkg-real: pkg_real_cli.o pkg_scanner.o pkg_parser.o pkg_dag.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

pkg_real_cli.o: pkg_real_cli.c pkg_scanner.h pkg_parser.h pkg_dag.h
	$(CC) $(CFLAGS) -c -o $@ $<

pkg_scanner.o: pkg_scanner.c pkg_scanner.h
	$(CC) $(CFLAGS) -c -o $@ $<

pkg_parser.o: pkg_parser.c pkg_parser.h
	$(CC) $(CFLAGS) -c -o $@ $<

pkg_dag.o: pkg_dag.c pkg_dag.h pkg_scanner.h pkg_parser.h
	$(CC) $(CFLAGS) -c -o $@ $<

test-pkg-real: tests/test_pkg_real.o pkg_scanner.o pkg_parser.o pkg_dag.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_pkg_real.o: tests/test_pkg_real.c pkg_scanner.h pkg_parser.h pkg_dag.h
	$(CC) $(CFLAGS) -c -o $@ $<

# REAL: metrics producer built with reduced-friction preset + provenance.
# Uses --gc-sections + LTO + hidden visibility so the linker discards
# every symbol not actually reachable from main().
# Provenance (git SHA, build time, toolchain, cflags fingerprint) is
# injected via -D so the binary can emit it into every JSON.
metrics-producer: metrics_producer.real.o pkg_scanner.real.o pkg_parser.real.o pkg_dag.real.o real_provenance.real.o real_arch.real.o real_receipt.real.o real_sha256.real.o
	$(CC) $(REAL_CFLAGS) -o $@ $^ $(REAL_LDFLAGS)

metrics_producer.real.o: metrics_producer.c pkg_scanner.h pkg_parser.h pkg_dag.h real_provenance.h real_contract.h real_arch.h real_receipt.h
	$(CC) $(REAL_CFLAGS) $(REAL_PROV_DEFS) -c -o $@ $<

pkg_scanner.real.o: pkg_scanner.c pkg_scanner.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

pkg_parser.real.o: pkg_parser.c pkg_parser.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

pkg_dag.real.o: pkg_dag.c pkg_dag.h pkg_scanner.h pkg_parser.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

real_provenance.real.o: real_provenance.c real_provenance.h
	$(CC) $(REAL_CFLAGS) $(REAL_PROV_DEFS) -c -o $@ $<

# REAL: contract validator — enforces pkg_metrics/1.0.0 schema fail-closed.
contract-validate: contract_validate_cli.real.o real_contract.real.o real_provenance.real.o
	$(CC) $(REAL_CFLAGS) -o $@ $^ $(REAL_LDFLAGS)

contract_validate_cli.real.o: contract_validate_cli.c real_contract.h real_provenance.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

real_contract.real.o: real_contract.c real_contract.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

# REAL: architecture matrix + auto-detection CLI
arch-detect: arch_detect_cli.real.o real_arch.real.o
	$(CC) $(REAL_CFLAGS) -o $@ $^ $(REAL_LDFLAGS)

arch_detect_cli.real.o: arch_detect_cli.c real_arch.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

real_arch.real.o: real_arch.c real_arch.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

# REAL: SHA256 + receipts + validator
real_sha256.real.o: real_sha256.c real_sha256.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

real_receipt.real.o: real_receipt.c real_receipt.h real_sha256.h real_provenance.h real_arch.h
	$(CC) $(REAL_CFLAGS) $(REAL_PROV_DEFS) -c -o $@ $<

receipt-validate: receipt_validate_cli.real.o real_receipt.real.o real_sha256.real.o real_provenance.real.o real_arch.real.o
	$(CC) $(REAL_CFLAGS) -o $@ $^ $(REAL_LDFLAGS)

receipt_validate_cli.real.o: receipt_validate_cli.c real_receipt.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

# REAL: chain-of-custody ledger (append-only, hash-linked receipts)
receipt-ledger: receipt_ledger_cli.real.o real_ledger.real.o real_receipt.real.o real_sha256.real.o real_provenance.real.o real_arch.real.o
	$(CC) $(REAL_CFLAGS) -o $@ $^ $(REAL_LDFLAGS)

receipt_ledger_cli.real.o: receipt_ledger_cli.c real_ledger.h real_receipt.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

real_ledger.real.o: real_ledger.c real_ledger.h real_receipt.h real_sha256.h
	$(CC) $(REAL_CFLAGS) -c -o $@ $<

# REAL Phase 10: runtime capability probe (OBSERVED supersedes nominal)
arch-probe: arch_probe_cli.real.o real_arch.real.o real_provenance.real.o real_receipt.real.o real_sha256.real.o
	$(CC) $(REAL_CFLAGS) -o $@ $^ $(REAL_LDFLAGS)

arch_probe_cli.real.o: arch_probe_cli.c real_arch.h real_provenance.h real_receipt.h
	$(CC) $(REAL_CFLAGS) $(REAL_PROV_DEFS) -c -o $@ $<

# REAL freestanding: NO libc, NO CRT, direct syscalls only.
# Warnings still act as linker commands via -fdata-sections + gc-sections.
# -no-pie is a LINK-time flag (clang warns if seen at compile-only with
# -Werror -Wunused-command-line-argument). Keep it only in LDFLAGS.
FREESTANDING_CFLAGS := -Wall -Wextra -Wshadow -Werror -O2 \
                       -nostdlib -nostartfiles -ffreestanding -static \
                       -fno-builtin -fno-stack-protector \
                       -fdata-sections -ffunction-sections \
                       -fvisibility=hidden
# -static implies no-pie behavior; drop -no-pie (clang warns even in
# link stage of a compile+link driver invocation because it evaluates
# unused args per compile job). This does NOT change the emitted
# binary — static ELF is always non-PIE.
FREESTANDING_LDFLAGS := -nostdlib -nostartfiles -static \
                        -Wl,--gc-sections -Wl,--build-id=none

pkg-count-freestanding: pkg_count_freestanding.c real_syscalls.h real_mem.h
	$(CC) $(FREESTANDING_CFLAGS) -o $@ $< $(FREESTANDING_LDFLAGS)

tests/test_cycle2_1_manifesto_v2.o: tests/test_cycle2_1_manifesto_v2.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_cycle2_2_source_download.o: tests/test_cycle2_2_source_download.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_cycle2_3_source_patch.o: ../tests/test_cycle2_3_source_patch.c
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -c -o $@ $<

patch_apply.o: patch_apply.c
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -c -o $@ $<

tests/test_build_orchestrator.o: tests/test_build_orchestrator.c build_orchestrator.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_dep_resolver.o: tests/test_dep_resolver.c dep_resolver.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_manifest_v2.o: tests/test_manifest_v2.c manifest_v2.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_cycle2_4_manifest_v2.o: ../tests/test_cycle2_4_manifest_v2.c manifest_v2.h
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -c -o $@ $<

manifest_v2.o: manifest_v2.c manifest_v2.h
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -c -o $@ $<

%.o: %.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

manifest: manifest_generator.py
	python3 manifest_generator.py ../packages manifest.bin

test: clean all
	python3 tests/test_manifest_fail_closed.py
	@echo "✓ Python manifest validation passed"
	./test-cycle2-1
	@echo "✓ Cycle 2.1 (manifest) validation passed"
	./test-cycle2-2
	@echo "✓ Cycle 2.2 (source download) validation passed"
	./test-build-orchestrator
	@echo "✓ Build orchestrator tests passed"
	./test-dep-resolver
	@echo "✓ Dependency resolver tests passed"
	./test-manifest-v2
	@echo "✓ Manifest V2 tests passed"
	./device-test-orchestrator
	@echo "✓ Device orchestrator tests passed"
	@echo "All tests completed successfully"
	./test-sprint3
	@echo "✓ Sprint 3 tests passed"
	./test-sprint4
	@echo "✓ Sprint 4 (multicore) tests passed"
	./test-sprint5
	@echo "✓ Sprint 5 (hardware tuning) tests passed"
	./test-sprint6
	@echo "✓ Sprint 6 (vectorization) tests passed"
	./test-sprint10
	@echo "✓ Sprint 10 (work stealing) tests passed"
	./test-sprint11
	@echo "✓ Sprint 11 (adaptive DVFS) tests passed"
	./test-sprint12
	@echo "✓ Sprint 12 (GPU integration) tests passed"
	./test-bitraf64-simd-ops
	@echo "✓ BITRAF64 SIMD operations tests passed"
	./test-bitraf64-lut-cache
	@echo "✓ BITRAF64 LUT cache tests passed"
	./test-bitraf64-coherence-monitor
	@echo "✓ BITRAF64 coherence monitor tests passed"
	./test-ascii-grafo
	@echo "✓ ASCII-Grafo tests passed"
	./test-e2e-build
	@echo "✓ End-to-End build tests passed"
	./test-distributed
	@echo "✓ Distributed builds tests passed"
	./test-performance-opt
	@echo "✓ Performance optimization tests passed"
	./test-security-hardening
	@echo "✓ Security hardening tests passed"
	./test-integration-e2e
	@echo "✓ Integration E2E tests passed"

test-sprint3: tests/test_sprint3.o cycle_budget.o workload_generator.o coherence_tuning.o profiler.o build_orchestrator.o dep_resolver.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

test-sprint4: tests/test_sprint4.o multicore_orchestrator.o build_orchestrator.o phase_barrier_lockfree.o dep_resolver.o profiler.o cycle_budget.o coherence_tuning.o workload_generator.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lpthread -lm

test-sprint5: tests/test_sprint5.o hardware_tuning.o cpu_affinity.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

test-sprint6: tests/test_sprint6.o advanced_vectorization.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

test-sprint10: tests/test_sprint10.o work_stealing.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

test-sprint11: tests/test_sprint11.o adaptive_dvfs.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

test-sprint12: tests/test_sprint12.o gpu_integration.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

tests/test_sprint3.o: tests/test_sprint3.c cycle_budget.h workload_generator.h coherence_tuning.h profiler.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_sprint4.o: tests/test_sprint4.c multicore_orchestrator.h build_orchestrator.h dep_resolver.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_sprint5.o: tests/test_sprint5.c hardware_tuning.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_sprint6.o: tests/test_sprint6.c advanced_vectorization.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_sprint10.o: tests/test_sprint10.c work_stealing.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_sprint11.o: tests/test_sprint11.c adaptive_dvfs.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_sprint12.o: tests/test_sprint12.c gpu_integration.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_ascii_grafo.o: tests/test_ascii_grafo.c ascii_grafo.h ascii_grafo_integration.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_e2e_build.o: tests/test_e2e_build.c workload_generator.h ascii_grafo_integration.h bitraf64_coherence_monitor.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_production_hardening.o: tests/test_production_hardening.c production_hardening.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_orchestrator_v2.o: tests/test_orchestrator_v2.c build_orchestrator_v2.h dep_resolver_v2.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_distributed.o: tests/test_distributed.c dist_orchestrator.h rpc_coordinator.h dist_checkpoint.h load_balancer.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_performance_opt.o: tests/test_performance_opt.c manifest_simd.h scheduler_adaptive.h alloc_pool.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_security_hardening.o: tests/test_security_hardening.c crypto_ed25519.h crypto_chacha20.h rate_limiter.h crypto_pqc.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_integration_e2e.o: tests/test_integration_e2e.c dist_orchestrator.h rpc_coordinator.h dist_checkpoint.h load_balancer.h crypto_ed25519.h crypto_chacha20.h rate_limiter.h crypto_pqc.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(DUMPER_OBJECTS) tests/test_manifest_v2.o tests/test_bitraf64_simd_ops.o tests/test_bitraf64_lut_cache.o tests/test_bitraf64_coherence_monitor.o tests/test_ascii_grafo.o tests/test_e2e_build.o tests/test_production_hardening.o tests/test_orchestrator_v2.o tests/test_distributed.o tests/test_performance_opt.o tests/test_security_hardening.o tests/test_integration_e2e.o build_orchestrator_optimized.o perf_analyzer.o *.real.o $(TARGETS) manifest.bin

# H5 fix: install target used $(PREFIX) with no default — when PREFIX
# was unset, $(PREFIX)/bin/... expanded to /bin/..., silently writing
# binaries into system root. Now derive PREFIX from Termux env if
# present, otherwise fail loud with a clear message. Fail-closed
# beats accidentally clobbering /bin/termux-build-core on the host.
INSTALL_PREFIX := $(or $(strip $(PREFIX)),$(strip $(TERMUX_PREFIX)),$(strip $(TERMUX__PREFIX)))

install: $(TARGETS)
	@if [ -z "$(INSTALL_PREFIX)" ]; then \
	    echo "BLOCKED: install requires PREFIX (or TERMUX_PREFIX/TERMUX__PREFIX)"; \
	    echo "  usage:  make install PREFIX=/usr/local"; \
	    echo "  termux: PREFIX/TERMUX_PREFIX auto-derived when set in env"; \
	    exit 2; \
	fi
	install -D -m 755 termux-build-core $(INSTALL_PREFIX)/bin/termux-build-core
	install -D -m 755 test-manifest $(INSTALL_PREFIX)/bin/test-manifest
	install -D -m 755 manifest-dumper $(INSTALL_PREFIX)/bin/manifest-dumper

# ============================================================================
# REAL layer convenience targets — build, test, or run the executable tour.
# ============================================================================

# Build every REAL binary in one shot.
real-all: metrics-producer arch-probe receipt-validate receipt-ledger \
          contract-validate arch-detect pkg-real pkg-count-freestanding
	@echo "✓ REAL binaries built:"
	@ls -l metrics-producer arch-probe receipt-validate receipt-ledger \
	       contract-validate arch-detect pkg-real pkg-count-freestanding

# Run every REAL test suite; fail-closed on any suite red.
# Runs from repo root because pkg_scanner needs to find packages/ dirs.
real-test: real-all
	@cd .. && ./core/test-pkg-real
	@cd .. && bash core/tests/test_real_attrs.sh
	@cd .. && bash core/tests/test_freestanding.sh
	@cd .. && bash core/tests/test_governance.sh
	@cd .. && bash core/tests/test_arch.sh
	@cd .. && bash core/tests/test_receipts.sh
	@cd .. && bash core/tests/test_ledger.sh
	@cd .. && bash core/tests/test_arch_probe.sh
	@cd .. && bash core/tests/test_hotfix_b1_receipt_aliasing.sh
	@cd .. && bash core/tests/test_hotfix_b3_b4_strto_guards.sh
	@cd .. && bash core/tests/test_hotfix_b6_ledger_concurrent.sh
	@cd .. && bash core/tests/test_hotfix_c17_json_escape.sh
	@cd .. && bash core/tests/test_hotfix_c23_ledger_rollback.sh
	@cd .. && bash core/tests/test_hotfix_c40_symlink_hardening.sh
	@cd .. && bash core/tests/test_hotfix_d3_d4_d5_json_roundtrip.sh
	@cd .. && bash core/tests/test_hotfix_e9_ledger_path_escape.sh
	@cd .. && bash core/tests/test_hotfix_f6_ledger_line_length.sh
	@cd .. && bash core/tests/test_hotfix_g6_producer_exit_code.sh
	@cd .. && bash core/tests/test_hotfix_h5_install_prefix.sh
	@echo "✓ All REAL test suites passed"

# Compile-hygiene suite lives in a separate target because each run
# does two full clean rebuilds (gcc + clang) and takes ~3 minutes.
# CI can invoke it independently after real-test.
real-hygiene:
	@cd .. && bash core/tests/test_hotfix_h_compile_hygiene.sh

# Run the executable documentation tour; refreshes docs/REAL_TOUR_OUTPUT.md
real-tour: real-all
	@bash ../docs/REAL_TOUR.sh

# Same as real-tour but non-zero exit on any failure (CI use).
real-tour-check: real-all
	@bash ../docs/REAL_TOUR.sh --check

# ============================================================================
# Cycle 2 Gates — Source Acquisition (TV-01 to TV-05)
# ============================================================================

cycle2-source-gate: test-cycle2-2
	@echo "=== TV-01: SOURCE_FETCH Gate ==="
	@echo "Repository: $(shell git rev-parse HEAD | cut -c1-12)"
	@echo "Build timestamp: $(shell date -u +%Y-%m-%dT%H:%M:%SZ)"
	@echo ""
	./test-cycle2-2
	@echo ""
	@echo "✓ TV-01 (SOURCE_FETCH) gate PASS"

cycle2-extract-gate: test-cycle2-2
	@echo "=== TV-02: SOURCE_EXTRACT Gate ==="
	@echo "Note: TV-02 extraction validated as part of TV-01 (source download includes extraction)"
	./test-cycle2-2
	@echo "✓ TV-02 (SOURCE_EXTRACT) gate PASS"

cycle2-patch-gate: test-cycle2-3
	@echo "=== TV-03: PATCH_APPLY Gate ==="
	@echo "Repository: $(shell git rev-parse HEAD | cut -c1-12)"
	@echo "Build timestamp: $(shell date -u +%Y-%m-%dT%H:%M:%SZ)"
	@echo ""
	./test-cycle2-3
	@echo ""
	@echo "✓ TV-03 (PATCH_APPLY) gate PASS"

cycle2-manifest-gate: test-cycle2-4
	@echo "=== TV-04: MANIFEST_BINDING Gate ==="
	@echo "Building and running manifest_v2 validation test..."
	@./test-cycle2-4
	@echo ""
	@echo "Receipt: TV-04 MANIFEST_BINDING gate PASS"
	@echo "  Repository: rafaelmeloreisnovo/termux-packages"
	@echo "  Commit: $(shell git rev-parse HEAD 2>/dev/null || echo 'unknown')"
	@echo "  Timestamp: $(shell date -u +'%Y-%m-%dT%H:%M:%SZ')"
	@echo "  Test: test-cycle2-4 (Manifesto V2 bounds checking)"
	@echo "  Exit code: 0 (PASS)"
	@echo ""

cycle2-dep-gate:
	@echo "=== TV-05: DEP_GRAPH Gate ==="
	@echo "Note: TV-05 (dependency resolution) is TOKEN_VAZIO - requires manifest reading"
	@echo "Status: GATE_BLOCKED_ON_TV-05_IMPLEMENTATION"
	@exit 1

# Pattern rules for compiling C files to object files
%.o: %.c
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -c -o $@ $^

# Explicit rule for tests directory
tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -c -o $@ $^
