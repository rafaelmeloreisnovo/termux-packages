CC := gcc
CFLAGS := -Wall -Wextra -Wshadow -g -O0
LDFLAGS :=

TARGETS := termux-build-core test-manifest
SOURCES := termux-build-core.c sysexec.c package_writer.c
TEST_SOURCES := test_manifest.c manifest_loader.c
OBJECTS := $(SOURCES:.c=.o)
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)

.PHONY: all clean test

all: $(TARGETS)

termux-build-core: termux-build-core.o sysexec.o package_writer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test-manifest: test_manifest.o manifest_loader.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c manifest.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: all manifest.bin
	./test-manifest 2>&1 | head -20
	@echo
	./termux-build-core --package hello-world --arch aarch64 --api 24 --manifest manifest.bin 2>&1 | head -15

clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) $(TARGETS)

install: $(TARGETS)
	install -D -m 755 termux-build-core $(PREFIX)/bin/termux-build-core
	install -D -m 755 test-manifest $(PREFIX)/bin/test-manifest
