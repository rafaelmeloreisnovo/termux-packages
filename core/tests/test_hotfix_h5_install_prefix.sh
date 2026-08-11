#!/usr/bin/env bash
# HOTFIX regression: H5 — `make install` with no PREFIX must fail
# LOUD instead of silently writing to /bin/ (system root).
#
# Pre-fix state:
#   Recipe used $(PREFIX)/bin/... — when PREFIX unset, expands to
#   /bin/... — accidentally installs into system root on the host.
#   If run as root during package build, could overwrite system
#   /bin/termux-build-core, /bin/test-manifest, /bin/manifest-dumper.
#
# Post-fix:
#   INSTALL_PREFIX derives from PREFIX or TERMUX_PREFIX or
#   TERMUX__PREFIX (in that order). If none set → BLOCKED with clear
#   message + exit 2. Never silently writes to /bin.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

SCRATCH="/tmp/real_h5_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

echo "=== HOTFIX H5: install PREFIX fail-closed regression ==="

# 1) No PREFIX set anywhere → must FAIL with non-zero exit
env -u PREFIX -u TERMUX_PREFIX -u TERMUX__PREFIX \
    make -C core install >"$SCRATCH/no_prefix.log" 2>&1
rc=$?
if [ "$rc" -ne 0 ] && grep -q "BLOCKED: install requires PREFIX" "$SCRATCH/no_prefix.log"; then
    pass "install fail-closed when PREFIX unset (exit $rc, message shown)"
else
    fail "install did NOT fail-closed: exit=$rc, output:"
    tail -3 "$SCRATCH/no_prefix.log" >&2
fi

# 2) System /bin must NOT have been written to
if [ ! -e /bin/termux-build-core-installtest ]; then
    pass "system /bin unchanged (no accidental write)"
else
    fail "/bin/termux-build-core-installtest was written (H5 regression)"
    rm -f /bin/termux-build-core-installtest
fi

# 3) With explicit PREFIX to scratch dir → must succeed AND install there
make -C core clean >/dev/null 2>&1
PREFIX="$SCRATCH/inst" make -C core install >"$SCRATCH/with_prefix.log" 2>&1
if [ -x "$SCRATCH/inst/bin/termux-build-core" ] && \
   [ -x "$SCRATCH/inst/bin/test-manifest" ] && \
   [ -x "$SCRATCH/inst/bin/manifest-dumper" ]; then
    pass "install with PREFIX=$SCRATCH/inst placed all 3 binaries"
else
    fail "install with explicit PREFIX missed binaries"
    ls -la "$SCRATCH/inst/bin/" 2>&1 >&2
fi

# 4) With TERMUX_PREFIX (auto-derive path) → same
make -C core clean >/dev/null 2>&1
env -u PREFIX TERMUX_PREFIX="$SCRATCH/tp" make -C core install >"$SCRATCH/with_tp.log" 2>&1
if [ -x "$SCRATCH/tp/bin/termux-build-core" ]; then
    pass "install with TERMUX_PREFIX auto-derived correctly"
else
    fail "install with TERMUX_PREFIX did not derive INSTALL_PREFIX"
fi

# 5) With TERMUX__PREFIX (double underscore variant) → same
make -C core clean >/dev/null 2>&1
env -u PREFIX -u TERMUX_PREFIX TERMUX__PREFIX="$SCRATCH/tp2" \
    make -C core install >"$SCRATCH/with_tp2.log" 2>&1
if [ -x "$SCRATCH/tp2/bin/termux-build-core" ]; then
    pass "install with TERMUX__PREFIX auto-derived correctly"
else
    fail "install with TERMUX__PREFIX did not derive INSTALL_PREFIX"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
