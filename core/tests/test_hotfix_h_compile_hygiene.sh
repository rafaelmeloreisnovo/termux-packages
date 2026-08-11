#!/usr/bin/env bash
# HOTFIX regression: H1..H4 — full-tree compile must be warning-clean
# under BOTH gcc and clang with -Werror. Pre-fix state (Pass 8):
#   H1: build_orchestrator_optimized.c:168 — `phase_transition_count`
#       static inline unused → clang -Werror,-Wunused-function
#   H2: build_orchestrator_simd.c:50, 136 — `phi_compute_simd` and
#       `simd_batch_add_package` unused → same class
#   H3: phase_barrier_lockfree.c:44 — `atomic_cas` unused → same class
#   H4: -no-pie in FREESTANDING_CFLAGS/LDFLAGS → clang
#       -Werror,-Wunused-command-line-argument (only valid at link;
#       -static already implies non-PIE)
#
# gcc did NOT catch these — gcc's -Wunused-function is quieter for
# `static inline`. Clang's -Werror flushed them out.
#
# This test does a full clean rebuild under both compilers and
# ensures the compile log has NO error and NO warning.
set -uo pipefail

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX H1..H4: full-tree compile hygiene (gcc + clang) ==="

for cc in gcc clang; do
    if ! command -v "$cc" >/dev/null 2>&1; then
        printf "  ⊘ %s not installed; skipping\n" "$cc"
        continue
    fi
    make -C core clean >/dev/null 2>&1
    log=$(mktemp)
    if CC="$cc" make -C core all >"$log" 2>&1; then
        # Count real diagnostic emissions only; skip lines that
        # merely mention `-Werror` on the compiler cmdline.
        n_err=$(grep -E "error:" "$log" | grep -v -- "-Werror" | wc -l || true)
        n_wrn=$(grep -E "warning:" "$log" | grep -v -- "-Werror" | wc -l || true)
        if [ "$n_err" -eq 0 ] && [ "$n_wrn" -eq 0 ]; then
            pass "$cc: full-tree build clean (0 errors, 0 warnings)"
        else
            fail "$cc: $n_err error(s), $n_wrn warning(s) in log"
            grep -E "error:|warning:" "$log" | head -5 >&2
        fi
    else
        fail "$cc: make all failed with non-zero exit"
        tail -20 "$log" >&2
    fi
    rm -f "$log"
done

# Note: we intentionally do NOT invoke `make real-test` here because
# this script itself is wired into that target — recursing would loop.
# The other test suites already verify REAL binaries end-to-end.

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
