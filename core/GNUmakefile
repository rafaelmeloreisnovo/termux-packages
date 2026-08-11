# RAFCODEPHI core build hygiene overlay.
#
# GNU make loads GNUmakefile before Makefile.  Keep the legacy build graph in
# Makefile, but constrain target selection before it is parsed so cross-target
# and Termux package builds do not accidentally compile host-only AVX-512
# benchmarks.  Also make the existing clean recipe see every object file,
# including REAL/pkg-real/test objects that are not listed in SOURCES.

BASE_MAKEFILE := Makefile
BASE_TARGETS := $(strip $(shell sed -n 's/^TARGETS := //p' $(BASE_MAKEFILE)))
CC_MACHINE := $(strip $(shell $(CC) -dumpmachine 2>/dev/null || printf '%s' unknown))
INSTALL_TARGETS := termux-build-core test-manifest manifest-dumper
NONPORTABLE_SIMD_TARGETS := advanced-simd-benchmark advanced-simd-test

# A Termux package build runs plain `make` before `make install`.  The install
# recipe only ships INSTALL_TARGETS, so compiling the full benchmark/test graph
# there is both unnecessary and unsafe for Android cross compilers.
ifneq ($(strip $(TERMUX_PREFIX)$(TERMUX__PREFIX)),)
override TARGETS := $(INSTALL_TARGETS)
else
# Outside the Termux packaging environment, preserve the complete graph on x86
# but remove the two explicitly AVX-512-only link targets for ARM/AArch64 and
# other non-x86 compiler triples.
ifeq ($(filter x86_64% amd64% i%86%,$(CC_MACHINE)),)
override TARGETS := $(filter-out $(NONPORTABLE_SIMD_TARGETS),$(BASE_TARGETS))
endif
endif

# `make install` must build only what its recipe actually installs.  Without
# this, the legacy `install: $(TARGETS)` prerequisite pulls AVX-512 benchmarks
# into an ARM/Android install for no reason.
ifneq ($(filter install,$(MAKECMDGOALS)),)
override TARGETS := $(INSTALL_TARGETS)
endif

# The legacy Makefile's clean target expands OBJECTS/TEST_OBJECTS.  Make those
# variables include every object present at execution time so compiler/ABI/LTO
# objects cannot leak from one build into the next.
override OBJECTS = $(SOURCES:.c=.o) $(wildcard *.o)
override TEST_OBJECTS = $(TEST_SOURCES:.c=.o) $(wildcard tests/*.o)

include $(BASE_MAKEFILE)

.PHONY: compilation-hygiene-test
compilation-hygiene-test:
	@bash tests/test_makefile_compilation_hygiene.sh
