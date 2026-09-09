#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROPERTIES="${ROOT_DIR}/scripts/properties.sh"
EXPECTED='TERMUX_APP__PACKAGE_NAME="com.termux"'
REPLACEMENT='TERMUX_APP__PACKAGE_NAME="com.termux.rafacodephi"'

[[ -f "$PROPERTIES" ]] || { echo "missing $PROPERTIES" >&2; exit 1; }

python3 - "$PROPERTIES" "$EXPECTED" "$REPLACEMENT" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
expected = sys.argv[2]
replacement = sys.argv[3]
text = path.read_text(encoding="utf-8")
count = text.count(expected)
if count != 1:
    raise SystemExit(
        f"fail-closed: expected exactly one canonical package-name assignment, found {count}"
    )
path.write_text(text.replace(expected, replacement, 1), encoding="utf-8")
PY

grep -Fqx "$REPLACEMENT" "$PROPERTIES" \
  || { echo "failed to configure RAFCODEPHI package name" >&2; exit 1; }

printf 'TERMUX_APP__PACKAGE_NAME=com.termux.rafacodephi\n'
printf 'TERMUX__PREFIX=/data/data/com.termux.rafacodephi/files/usr (derived by upstream properties)\n'
printf 'source_properties_sha256=%s\n' "$(sha256sum "$PROPERTIES" | awk '{print $1}')"
