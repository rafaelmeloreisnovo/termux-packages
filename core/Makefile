CC ?= cc
CFLAGS ?= -Wall -Wextra -Wshadow -Werror -g -O0
LDFLAGS ?=

TARGETS := termux-build-core test-manifest manifest-dumper
SOURCES := termux-build-core.c sysexec.c package_writer.c elf_writer.c friction_analyzer.c checkpoint.c parallel_jobs.c build_exec.c manifest_loader.c
TEST_SOURCES := test_manifest.c manifest_loader.c
DUMPER_SOURCES := manifest_dumper.c
OBJECTS := $(SOURCES:.c=.o)
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)
DUMPER_OBJECTS := $(DUMPER_SOURCES:.c=.o)

.PHONY: all clean test manifest

all: $(TARGETS)

termux-build-core: termux-build-core.o sysexec.o package_writer.o elf_writer.o friction_analyzer.o checkpoint.o parallel_jobs.o build_exec.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-manifest: test_manifest.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

manifest-dumper: manifest_dumper.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

manifest: manifest_generator.py
	python3 manifest_generator.py ../packages manifest.bin

test: clean all
	python3 tests/test_manifest_fail_closed.py
	@echo "Structural and fixture manifest gates passed. Run 'make manifest' separately for the global V1 contract."

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(DUMPER_OBJECTS) $(TARGETS) manifest.bin

install: $(TARGETS)
	install -D -m 755 termux-build-core $(PREFIX)/bin/termux-build-core
	install -D -m 755 test-manifest $(PREFIX)/bin/test-manifest
	install -D -m 755 manifest-dumper $(PREFIX)/bin/manifest-dumper
