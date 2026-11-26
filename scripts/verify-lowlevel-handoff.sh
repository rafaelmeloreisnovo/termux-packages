#!/usr/bin/env bash
set -euo pipefail
root=${1:-.}
manifest="$root/docs/assurance/lowlevel-selection.v1.json"
license="$root/docs/assurance/RAFCODEPHI_STUDY_RESEARCH_LICENSE.md"
test -f "$manifest" || { echo "MISSING:$manifest"; exit 20; }
test -f "$license" || { echo "MISSING:$license"; exit 20; }
grep -q '"claim_allowed": false' "$manifest"
grep -q '"PURE_CORE"' "$manifest"
grep -q '"PLATFORM_GATE"' "$manifest"
grep -q '"HOSTED_BOUNDARY"' "$manifest"
grep -q '"status": "PRODUCER_CONTRACT_ONLY_PENDING_BUILD"' "$manifest"
printf '%s\n' 'PACKAGE_LOWLEVEL_HANDOFF_STATIC_PASS'
