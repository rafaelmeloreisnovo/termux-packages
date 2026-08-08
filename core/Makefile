CC ?= cc
CFLAGS ?= -Wall -Wextra -Wshadow -Werror -g -O0
LDFLAGS ?=

TARGETS := termux-build-core test-manifest manifest-dumper test-cycle2-1 test-cycle2-2 test-build-orchestrator test-dep-resolver test-manifest-v2 device-test-orchestrator perf-profiler perf-analyzer simd-benchmark
SOURCES := termux-build-core.c sysexec.c package_writer.c elf_writer.c friction_analyzer.c checkpoint.c parallel_jobs.c build_exec.c manifest_loader.c source_download.c build_orchestrator.c dep_resolver.c manifest_v2.c device_test_orchestrator.c perf_profiler.c build_orchestrator_optimized.c perf_analyzer.c build_orchestrator_simd.c
TEST_SOURCES := test_manifest.c manifest_loader.c
DUMPER_SOURCES := manifest_dumper.c
OBJECTS := $(SOURCES:.c=.o)
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)
DUMPER_OBJECTS := $(DUMPER_SOURCES:.c=.o)

.PHONY: all clean test manifest

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

tests/test_cycle2_1_manifesto_v2.o: tests/test_cycle2_1_manifesto_v2.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_cycle2_2_source_download.o: tests/test_cycle2_2_source_download.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_build_orchestrator.o: tests/test_build_orchestrator.c build_orchestrator.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_dep_resolver.o: tests/test_dep_resolver.c dep_resolver.h
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test_manifest_v2.o: tests/test_manifest_v2.c manifest_v2.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

manifest: manifest_generator.py
	python3 manifest_generator.py ../packages manifest.bin

test: clean all test-cycle2-1
	python3 tests/test_manifest_fail_closed.py
	./test-cycle2-1
	@echo "Structural and fixture manifest gates passed. Cycle 2.1 validation complete."

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(DUMPER_OBJECTS) tests/test_manifest_v2.o build_orchestrator_optimized.o perf_analyzer.o $(TARGETS) manifest.bin

install: $(TARGETS)
	install -D -m 755 termux-build-core $(PREFIX)/bin/termux-build-core
	install -D -m 755 test-manifest $(PREFIX)/bin/test-manifest
	install -D -m 755 manifest-dumper $(PREFIX)/bin/manifest-dumper
