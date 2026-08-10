#!/usr/bin/env bash
# HOTFIX regression: C17 — JSON output from arch_probe must survive
# jq parsing (proves all string fields are properly escaped). Same
# for pkg_scanner inventory JSON.
set -uo pipefail

SCRATCH="/tmp/real_c17_test.$$"
mkdir -p "$SCRATCH"
trap 'rm -rf "$SCRATCH"' EXIT

PASSED=0
FAILED=0
pass() { printf "  ✓ %s\n" "$1"; PASSED=$((PASSED + 1)); }
fail() { printf "  ✗ %s\n" "$1"; FAILED=$((FAILED + 1)); }

echo "=== HOTFIX C17: JSON escape regression ==="

# arch-probe output must parse as JSON
core/arch-probe "$SCRATCH/cap.json" >/dev/null 2>&1
if [ -f "$SCRATCH/cap.json" ]; then
    pass "arch-probe wrote capability JSON"
else
    fail "arch-probe did not write output"; exit 1
fi

if jq -e . "$SCRATCH/cap.json" >/dev/null 2>&1; then
    pass "arch-probe JSON parses with jq"
else
    fail "arch-probe JSON malformed"
fi

# uname fields must be present + non-empty
sysname=$(jq -r '.identity.uname_sysname' "$SCRATCH/cap.json")
release=$(jq -r '.identity.uname_release' "$SCRATCH/cap.json")
machine=$(jq -r '.identity.uname_machine' "$SCRATCH/cap.json")
if [ -n "$sysname" ] && [ -n "$release" ] && [ -n "$machine" ]; then
    pass "uname fields extracted: $sysname/$release/$machine"
else
    fail "uname fields missing or empty in JSON"
fi

# pkg-real inventory JSON must parse
core/pkg-real inventory . --json 2>/dev/null > "$SCRATCH/inv.json" || true
if [ -s "$SCRATCH/inv.json" ] && jq -e . "$SCRATCH/inv.json" >/dev/null 2>&1; then
    pass "pkg-real inventory JSON parses with jq"
else
    fail "pkg-real inventory JSON malformed"
fi

# packages array must be present and non-empty (proves the escape
# didn't break the entries)
n_pkgs=$(jq '.packages | length' "$SCRATCH/inv.json" 2>/dev/null || echo 0)
if [ "$n_pkgs" -gt 0 ]; then
    pass "inventory JSON has $n_pkgs packages, each parseable"
else
    fail "no packages in inventory JSON (escape broke output)"
fi

# Every package must have parseable name/path/parent
missing=$(jq -r '.packages[] | select(.name == null or .path == null or .parent == null) | .name' "$SCRATCH/inv.json" 2>/dev/null | wc -l)
if [ "$missing" -eq 0 ]; then
    pass "all packages have non-null name/path/parent"
else
    fail "$missing packages missing required fields"
fi

echo ""
echo "=== Summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
[ "$FAILED" -eq 0 ]
