#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

REPOSITORY_URL="${RAFCODEPHI_APT_REPOSITORY_URL:-}"
TRUST_MODE="${RAFCODEPHI_APT_TRUST_MODE:-development-trusted}"

if [[ -z "$REPOSITORY_URL" ]]; then
    echo "RAFCODEPHI_APT_REPOSITORY_URL is required; refusing to create a bootstrap that pretends pkg/apt is usable." >&2
    exit 2
fi
case "$REPOSITORY_URL" in https://*) ;; *) echo "repository URL must use https" >&2; exit 2 ;; esac
if [[ "$TRUST_MODE" != "development-trusted" ]]; then
    echo "Only development-trusted is implemented. Production trust remains TOKEN_VAZIO." >&2
    exit 2
fi

# Do not depend on the executable bit surviving checkout/container bind mounts.
bash "$ROOT/scripts/build-rafcodephi-real-bootstrap.sh" "$@"
OUT_DIR="${RAFCODEPHI_BOOTSTRAP_OUT_DIR:-$ROOT/artifacts/rafcodephi-bootstrap}"

python3 - "$OUT_DIR" "$REPOSITORY_URL" "$TRUST_MODE" <<'PY'
import hashlib
import json
import os
import sys
import zipfile
from pathlib import Path

out_dir = Path(sys.argv[1])
repository_url = sys.argv[2].rstrip('/')
trust_mode = sys.argv[3]
source_path = "etc/apt/sources.list.d/termux.sources"
block_path = "etc/apt/apt.conf.d/00rafcodephi-repository-block"
source_payload = (
    "# RAFCODEPHI_PACKAGE_REPOSITORY=DEVELOPMENT_REPOSITORY_CONFIGURED\n"
    "Enabled: yes\nTypes: deb\n"
    f"URIs: {repository_url}\n"
    "Suites: ./\nTrusted: yes\n"
    "# Development boundary: HTTPS transport only; production signing root is TOKEN_VAZIO.\n"
).encode()
block_payload = (
    "// RAFCODEPHI development repository configured.\n"
    "// No APT::Update::Pre-Invoke blocker is active.\n"
    "// claim_allowed_release=false; production signing trust=TOKEN_VAZIO.\n"
).encode()

artifacts = {}
for path in sorted(out_dir.glob("rafcodephi-bootstrap-*.zip")):
    arch = path.stem.removeprefix("rafcodephi-bootstrap-")
    if arch not in {"arm", "aarch64"}:
        continue
    tmp = path.with_suffix(path.suffix + ".live.tmp")
    with zipfile.ZipFile(path, "r") as source, zipfile.ZipFile(tmp, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as target:
        names = set(source.namelist())
        required = {source_path, block_path, "BOOTSTRAP_PROFILE.json", "BOOTSTRAP_INFO"}
        missing = required - names
        if missing:
            raise SystemExit(f"{path.name}: missing entries: {sorted(missing)}")
        for info in source.infolist():
            payload = source.read(info.filename)
            if info.filename == source_path:
                payload = source_payload
            elif info.filename == block_path:
                payload = block_payload
            elif info.filename == "BOOTSTRAP_PROFILE.json":
                profile = json.loads(payload.decode())
                profile.update({
                    "package_repo_runtime_state": "DEVELOPMENT_REPOSITORY_CONFIGURED",
                    "apt_update_guard": "DISABLED_DEV_REPOSITORY_CONFIGURED",
                    "apt_repository_url": repository_url,
                    "apt_repository_trust": "GITHUB_HTTPS_TRANSPORT_ONLY",
                    "apt_repository_trust_mode": trust_mode,
                    "claim_allowed": False,
                    "release_allowed": False,
                    "device_validation": "TOKEN_VAZIO",
                })
                payload = (json.dumps(profile, sort_keys=True, separators=(",", ":")) + "\n").encode()
            elif info.filename == "BOOTSTRAP_INFO":
                rows = {}
                for line in payload.decode().splitlines():
                    if "=" in line:
                        k, v = line.split("=", 1)
                        rows[k] = v
                rows.update({
                    "RAFCODEPHI_PACKAGE_REPO_STATE": "DEVELOPMENT_REPOSITORY_CONFIGURED",
                    "RAFCODEPHI_APT_REPOSITORY_URL": repository_url,
                    "RAFCODEPHI_APT_REPOSITORY_TRUST": "GITHUB_HTTPS_TRANSPORT_ONLY",
                    "RAFCODEPHI_APT_UPDATE_GUARD": "DISABLED_DEV_REPOSITORY_CONFIGURED",
                    "RAFCODEPHI_DEVICE_VALIDATION": "TOKEN_VAZIO",
                    "RAFCODEPHI_CLAIM_ALLOWED": "0",
                })
                payload = "".join(f"{k}={rows[k]}\n" for k in sorted(rows)).encode()
            target.writestr(info, payload)
    os.replace(tmp, path)

    with zipfile.ZipFile(path, "r") as zf:
        source_text = zf.read(source_path).decode()
        profile = json.loads(zf.read("BOOTSTRAP_PROFILE.json").decode())
        block = zf.read(block_path).decode()
        if "Enabled: yes" not in source_text or f"URIs: {repository_url}" not in source_text:
            raise SystemExit(f"{path.name}: live apt source validation failed")
        if "APT::Update::Pre-Invoke" in block:
            raise SystemExit(f"{path.name}: old apt blocker still active")
        if profile.get("profile") != "real-pkg" or profile.get("package_layer") != "real-pkg":
            raise SystemExit(f"{path.name}: real-pkg contract regressed")
        if profile.get("package_repo_runtime_state") != "DEVELOPMENT_REPOSITORY_CONFIGURED":
            raise SystemExit(f"{path.name}: repository state mismatch")
        if profile.get("runtime_materialized") is not False:
            raise SystemExit(f"{path.name}: archive must not claim installed runtime materialization")
        if profile.get("claim_allowed") is not False or profile.get("release_allowed") is not False:
            raise SystemExit(f"{path.name}: claim boundary regressed")
    artifacts[arch] = (hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size)
    print(f"LIVE_BOOTSTRAP_PATCH=PASS arch={arch} artifact={path}")

if not artifacts:
    raise SystemExit("no live bootstrap artifacts found")
manifest = out_dir / "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
rows = {}
for raw in manifest.read_text(encoding="utf-8").splitlines():
    if not raw or raw.startswith("#"):
        continue
    if "=" not in raw:
        raise SystemExit(f"invalid manifest line: {raw!r}")
    k, v = raw.split("=", 1)
    if k in rows:
        raise SystemExit(f"duplicate manifest key before live reseal: {k}")
    rows[k] = v
rows.update({
    "package_repo_runtime_state": "DEVELOPMENT_REPOSITORY_CONFIGURED",
    "apt_update_guard": "DISABLED_DEV_REPOSITORY_CONFIGURED",
    "apt_repository_url": repository_url,
    "apt_repository_trust": "GITHUB_HTTPS_TRANSPORT_ONLY",
    "apt_repository_trust_mode": trust_mode,
    "claim_allowed_release": "false",
    "claim_allowed_device_runtime": "false",
    "device_runtime_proof": "TOKEN_VAZIO",
})
for arch, (digest, size) in artifacts.items():
    rows[f"sha256_{arch}"] = digest
    rows[f"bytes_{arch}"] = str(size)
manifest.write_text("".join(f"{k}={rows[k]}\n" for k in sorted(rows)), encoding="utf-8")
print(f"LIVE_MANIFEST_RESEAL=PASS manifest={manifest}")
PY

for artifact in "$OUT_DIR"/rafcodephi-bootstrap-*.zip; do
    [[ -s "$artifact" ]] || continue
    sha256sum "$artifact"
done

echo "RAFCODEPHI_LIVE_BOOTSTRAP=PASS"
echo "claim_allowed_release=false"
echo "device_runtime_proof=TOKEN_VAZIO"
