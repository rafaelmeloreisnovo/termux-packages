#!/usr/bin/env bash
# Local source digest falsifier only. No private data or device execution.
set -euo pipefail
test "${RAF_FALSIFIER_ID:-}" = SOURCE_DIGEST_NEGATIVE
test -n "${RAF_FALSIFIER_SHA:-}"
test -s "${RAF_FALSIFIER_ARTIFACT:-/not-a-file}"
python3 - "${RAF_FALSIFIER_ARTIFACT}" <<'PY'
import hashlib
from pathlib import Path
import subprocess
import sys
path = Path(sys.argv[1])
data = path.read_bytes()
assert 0 < len(data) <= 128*1024*1024
digest = hashlib.sha256(data).hexdigest()
external = subprocess.run(["sha256sum",str(path)],check=True,text=True,capture_output=True).stdout.split()[0]
assert digest == external
mutated = bytearray(data)
mutated[0] ^= 1
assert hashlib.sha256(mutated).hexdigest() != digest
PY
