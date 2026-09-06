#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ARCHITECTURES="arm,aarch64"
OUT="${RAFCODEPHI_LOCAL_PKG_OUT:-$ROOT/artifacts/rafcodephi-local-pkg}"
LKG_STAGE="$OUT/lkg"
CANDIDATE_STAGE="$OUT/candidate"
RECEIPTS="$OUT/receipts"

usage() {
  echo "Usage: $0 [--architectures arm|aarch64|arm,aarch64] [--out DIR]" >&2
}

while (($#)); do
  case "$1" in
    --architectures) ARCHITECTURES="${2:?missing architectures}"; shift 2 ;;
    --out) OUT="${2:?missing out}"; LKG_STAGE="$OUT/lkg"; CANDIDATE_STAGE="$OUT/candidate"; RECEIPTS="$OUT/receipts"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) usage; exit 2 ;;
  esac
done

mkdir -p "$LKG_STAGE" "$CANDIDATE_STAGE" "$RECEIPTS"

# 1) Produce the already-audited source-built real-pkg bootstrap. This remains
# untouched and is the rollback/LKG input.
./scripts/build-rafcodephi-real-bootstrap.sh \
  --architectures "$ARCHITECTURES" \
  --out "$LKG_STAGE"

# 2) Build packages required by the actual device smoke from the same RAFCODEPHI
# source/prefix overlay. Dependencies are source-built by termux-packages.
IFS=',' read -r -a ARCHES <<< "$ARCHITECTURES"
for arch in "${ARCHES[@]}"; do
  [[ "$arch" == "arm" || "$arch" == "aarch64" ]] || { echo "unsupported arch=$arch" >&2; exit 2; }
  ./build-package-rafcodephi.sh -a "$arch" nano python git
  test -s "$LKG_STAGE/rafcodephi-bootstrap-${arch}.zip"

done

# 3) Promote copies only. The persistent external apt source remains disabled;
# the pkg adapter explicitly selects the embedded hash-bound file: repository.
for arch in "${ARCHES[@]}"; do
  python3 scripts/promote-rafcodephi-local-pkg-candidate.py \
    --arch "$arch" \
    --source-zip "$LKG_STAGE/rafcodephi-bootstrap-${arch}.zip" \
    --packages-dir output \
    --output-zip "$CANDIDATE_STAGE/rafcodephi-bootstrap-${arch}.zip" \
    --receipt "$RECEIPTS/local-pkg-${arch}.json"
done

python3 - "$ARCHITECTURES" "$CANDIDATE_STAGE" "$RECEIPTS" "$OUT/matrix.v1.json" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

arches = [x for x in sys.argv[1].split(',') if x]
candidate = Path(sys.argv[2])
receipts = Path(sys.argv[3])
out = Path(sys.argv[4])

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()

rows = {}
for arch in arches:
    z = candidate / f'rafcodephi-bootstrap-{arch}.zip'
    r = receipts / f'local-pkg-{arch}.json'
    doc = json.loads(r.read_text())
    if doc.get('state') != 'STRUCTURAL_CANDIDATE' or doc.get('arch') != arch:
        raise SystemExit(f'invalid receipt {r}')
    if sha(z) != doc.get('candidate_zip_sha256'):
        raise SystemExit(f'hash mismatch {z}')
    rows[arch] = {
        'zip': str(z),
        'sha256': sha(z),
        'receipt': str(r),
        'receipt_sha256': sha(r),
        'repository_package_count': doc.get('repository_package_count'),
    }
payload = {
    'schema': 'rafcodephi.local-pkg-matrix/v1',
    'state': 'STRUCTURAL_CANDIDATE',
    'architectures': rows,
    'rollback': 'lkg source-built real-pkg bootstrap retained unchanged',
    'claim_allowed_pkg_runtime': False,
    'claim_allowed_device_runtime': False,
    'device_validation': 'TOKEN_VAZIO',
    'next_required_action': 'TERMUX_APP_IMPORT_THEN_DEVICE_REAL_PKG_SMOKE',
}
out.write_text(json.dumps(payload, sort_keys=True, indent=2) + '\n')
print(json.dumps(payload, sort_keys=True))
PY

printf 'RAFCODEPHI_LOCAL_PKG_MATRIX=STRUCTURAL_CANDIDATE device_validation=TOKEN_VAZIO out=%s\n' "$OUT"
