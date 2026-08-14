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
case "$REPOSITORY_URL" in
    https://*) ;;
    *) echo "repository URL must use https" >&2; exit 2 ;;
esac
if [[ "$TRUST_MODE" != "development-trusted" ]]; then
    echo "Only development-trusted is implemented here. Production trust remains TOKEN_VAZIO until a persistent signing root exists." >&2
    exit 2
fi

"$ROOT/scripts/build-rafcodephi-real-bootstrap.sh" "$@"
OUT_DIR="${RAFCODEPHI_BOOTSTRAP_OUT_DIR:-$ROOT/artifacts/rafcodephi-bootstrap}"

python3 - "$OUT_DIR" "$REPOSITORY_URL" "$TRUST_MODE" <<'PY'
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
    "Enabled: yes\n"
    "Types: deb\n"
    f"URIs: {repository_url}\n"
    "Suites: ./\n"
    "Trusted: yes\n"
    "# Security boundary: GitHub HTTPS transport only; not a production package-signing trust root.\n"
).encode("utf-8")
block_payload = (
    "// RAFCODEPHI development repository configured.\n"
    "// No Pre-Invoke blocker is installed in this beta bootstrap.\n"
    "// claim_allowed_release=false; production signing trust remains TOKEN_VAZIO.\n"
).encode("utf-8")

for path in sorted(out_dir.glob("rafcodephi-bootstrap-*.zip")):
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
                profile = json.loads(payload.decode("utf-8"))
                profile["package_repo_runtime_state"] = "DEVELOPMENT_REPOSITORY_CONFIGURED"
                profile["apt_update_guard"] = "DISABLED_DEV_REPOSITORY_CONFIGURED"
                profile["apt_repository_url"] = repository_url
                profile["apt_repository_trust"] = "GITHUB_HTTPS_TRANSPORT_ONLY"
                profile["apt_repository_trust_mode"] = trust_mode
                profile["claim_allowed"] = False
                profile["release_allowed"] = False
                profile["device_validation"] = "TOKEN_VAZIO"
                payload = (json.dumps(profile, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
            elif info.filename == "BOOTSTRAP_INFO":
                rows = {}
                for line in payload.decode("utf-8").splitlines():
                    if "=" in line:
                        k, v = line.split("=", 1)
                        rows[k] = v
                rows["RAFCODEPHI_PACKAGE_REPO_STATE"] = "DEVELOPMENT_REPOSITORY_CONFIGURED"
                rows["RAFCODEPHI_APT_REPOSITORY_URL"] = repository_url
                rows["RAFCODEPHI_APT_REPOSITORY_TRUST"] = "GITHUB_HTTPS_TRANSPORT_ONLY"
                rows["RAFCODEPHI_APT_UPDATE_GUARD"] = "DISABLED_DEV_REPOSITORY_CONFIGURED"
                rows["RAFCODEPHI_DEVICE_VALIDATION"] = "TOKEN_VAZIO"
                rows["RAFCODEPHI_CLAIM_ALLOWED"] = "0"
                payload = "".join(f"{k}={rows[k]}\n" for k in sorted(rows)).encode("utf-8")
            target.writestr(info, payload)
    os.replace(tmp, path)
    with zipfile.ZipFile(path, "r") as zf:
        source_text = zf.read(source_path).decode("utf-8")
        profile = json.loads(zf.read("BOOTSTRAP_PROFILE.json").decode("utf-8"))
        block = zf.read(block_path).decode("utf-8")
        if "Enabled: yes" not in source_text or f"URIs: {repository_url}" not in source_text:
            raise SystemExit(f"{path.name}: live apt source validation failed")
        if "Pre-Invoke" in block:
            raise SystemExit(f"{path.name}: fail-closed repository blocker still active")
        if profile.get("profile") != "real-pkg" or profile.get("package_layer") != "real-pkg":
            raise SystemExit(f"{path.name}: real-pkg contract regressed")
        if profile.get("package_repo_runtime_state") != "DEVELOPMENT_REPOSITORY_CONFIGURED":
            raise SystemExit(f"{path.name}: repository state mismatch")
        if profile.get("claim_allowed") is not False or profile.get("release_allowed") is not False:
            raise SystemExit(f"{path.name}: claim boundary regressed")
    print(f"LIVE_BOOTSTRAP_PATCH=PASS artifact={path}")

manifest = out_dir / "RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
with manifest.open("a", encoding="utf-8") as stream:
    stream.write("live_package_repo_state=DEVELOPMENT_REPOSITORY_CONFIGURED\n")
    stream.write(f"live_package_repo_url={repository_url}\n")
    stream.write("live_package_repo_trust=GITHUB_HTTPS_TRANSPORT_ONLY\n")
    stream.write("claim_allowed_release=false\n")
    stream.write("device_runtime_proof=TOKEN_VAZIO\n")
PY

for artifact in "$OUT_DIR"/rafcodephi-bootstrap-*.zip; do
    [[ -s "$artifact" ]] || continue
    sha256sum "$artifact"
done

echo "RAFCODEPHI_LIVE_BOOTSTRAP=PASS"
echo "claim_allowed_release=false"
echo "device_runtime_proof=TOKEN_VAZIO"
