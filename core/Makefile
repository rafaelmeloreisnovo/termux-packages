CC := gcc
CFLAGS := -Wall -Wextra -Wshadow -g -O0
LDFLAGS :=

TARGETS := termux-build-core test-manifest manifest-dumper
SOURCES := termux-build-core.c sysexec.c package_writer.c elf_writer.c friction_analyzer.c checkpoint.c parallel_jobs.c build_exec.c
TEST_SOURCES := test_manifest.c manifest_loader.c
DUMPER_SOURCES := manifest_dumper.c
OBJECTS := $(SOURCES:.c=.o)
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)
DUMPER_OBJECTS := $(DUMPER_SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGETS)

termux-build-core: termux-build-core.o sysexec.o package_writer.o elf_writer.o friction_analyzer.o checkpoint.o parallel_jobs.o build_exec.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-manifest: test_manifest.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

manifest-dumper: manifest_dumper.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: all manifest.bin
	./test-manifest 2>&1 | head -20
	@echo
	./termux-build-core --package hello-world --arch aarch64 --api 24 --manifest manifest.bin 2>&1 | head -15

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(DUMPER_OBJECTS) $(TARGETS)

install: $(TARGETS)
	install -D -m 755 termux-build-core $(PREFIX)/bin/termux-build-core
	install -D -m 755 test-manifest $(PREFIX)/bin/test-manifest
	install -D -m 755 manifest-dumper $(PREFIX)/bin/manifest-dumper
