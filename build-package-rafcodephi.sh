#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_ROOT"

RECEIPT="${RAFCODEPHI_PROPERTIES_RECEIPT:-build/reports/rafcodephi-build-properties-overlay.json}"
mkdir -p "$(dirname "$RECEIPT")"

python3 scripts/apply-rafcodephi-build-properties.py \
  --properties scripts/properties.sh \
  --receipt "$RECEIPT"

bash scripts/validate-rafcodephi-build-properties.sh

before_exec_sha="$(sha256sum scripts/properties.sh | awk '{print $1}')"
receipt_sha="$(python3 - "$RECEIPT" <<'PY'
import json, sys
print(json.load(open(sys.argv[1], encoding='utf-8'))['sha256_after'])
PY
)"
[[ "$before_exec_sha" == "$receipt_sha" ]] || {
  printf 'RAFCODEPHI_SAME_OBSERVATION=FAIL expected=%s observed=%s\n' "$receipt_sha" "$before_exec_sha" >&2
  exit 1
}

printf 'RAFCODEPHI_SAME_OBSERVATION=PASS properties_sha256=%s claim_allowed=false\n' "$before_exec_sha"
exec ./build-package.sh "$@"
