#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ARCHITECTURES="arm,aarch64"
DEB_ROOT="$ROOT/inputs"
SOURCE_COMMIT="${GITHUB_SHA:-$(git rev-parse HEAD)}"

usage() {
	printf 'Usage: %s [--architectures arm,aarch64] [--deb-root DIR]\n' "$0"
}

while (($#)); do
	case "$1" in
		--architectures) ARCHITECTURES="${2:?missing architecture list}"; shift 2 ;;
		--deb-root) DEB_ROOT="${2:?missing DEB root}"; shift 2 ;;
		-h|--help) usage; exit 0 ;;
		*) printf 'unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
	esac
done

case ",$ARCHITECTURES," in
	*,arm,*|*,aarch64,*) ;;
	*) printf 'only arm and aarch64 are supported\n' >&2; exit 2 ;;
esac

IFS=',' read -r -a arch_list <<< "$ARCHITECTURES"
for arch in "${arch_list[@]}"; do
	[[ "$arch" == arm || "$arch" == aarch64 ]] || { printf 'unsupported architecture: %s\n' "$arch" >&2; exit 2; }
	deb_dir="$(realpath -m "$DEB_ROOT/$arch/debs")"
	[[ -d "$deb_dir" ]] || { printf 'base DEB directory missing: %s\n' "$deb_dir" >&2; exit 1; }
	if [[ -s "$deb_dir/SHA256SUMS" ]]; then
		(cd "$deb_dir" && sha256sum -c SHA256SUMS)
	fi

	(
		export TERMUX_OUTPUT_DIR="$deb_dir"
		status="$DEB_ROOT/$arch/rafcodephi-developer-build-status.json"
		write_status() {
			python3 - "$status" "$arch" "$1" "$2" "$SOURCE_COMMIT" <<'PY'
import json, pathlib, sys
path, arch, package, state, commit = sys.argv[1:]
pathlib.Path(path).write_text(json.dumps({
    "schema": "rafcodephi.developer-build-status/v1",
    "architecture": arch,
    "package": package,
    "state": state,
    "source_commit": commit,
    "physical_android_runtime": "TOKEN_VAZIO",
    "claim_allowed": False,
}, indent=2, sort_keys=True) + "\n")
PY
		}
		# Build the executable roots first. ndk-sysroot is deliberately last:
		# installing it in the build prefix before LLVM is complete breaks LLVM.
		for package in nodejs-lts npm openjdk-21 android-tools libllvm make ninja \
			resolv-conf proot proot-distro rafcodephi-developer-profile ndk-sysroot; do
			write_status "$package" BUILDING
			./build-package.sh -a "$arch" "$package"
		done
		write_status COMPLETE PASS_STATIC_BUILD
	)

	python3 scripts/verify_rafcodephi_developer_debs.py \
		--deb-dir "$deb_dir" \
		--arch "$arch" \
		--source-commit "$SOURCE_COMMIT" \
		--receipt "$DEB_ROOT/$arch/rafcodephi-developer-static-gate.json"
	python3 - "$DEB_ROOT/$arch/receipt.json" "$deb_dir" \
		"$DEB_ROOT/$arch/rafcodephi-developer-static-gate.json" \
		"$arch" "$SOURCE_COMMIT" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

receipt_path, deb_dir, gate_path = map(Path, sys.argv[1:4])
arch, source_commit = sys.argv[4:6]
parent = json.loads(receipt_path.read_text(encoding="utf-8"))
files = {}
for path in sorted(deb_dir.glob("*.deb")):
    files["debs/" + path.name] = {
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "bytes": path.stat().st_size,
    }
gate_sha = hashlib.sha256(gate_path.read_bytes()).hexdigest()
receipt = {
    "schema": "rafcodephi.developer-handoff/v1",
    "architecture": arch,
    "input_artifact_sha256": parent["input_artifact_sha256"],
    "parent_schema": parent["schema"],
    "parent_source_commit": parent["source_commit"],
    "developer_source_commit": source_commit,
    "developer_static_gate_sha256": gate_sha,
    "files": files,
    "profile": "rafcodephi-developer-profile",
    "device_runtime": "TOKEN_VAZIO",
    "claim_allowed": False,
    "apt_repository_enabled": False,
}
receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
PY
	(
		cd "$deb_dir"
		find . -maxdepth 1 -type f -name '*.deb' -print0 \
			| sort -z | xargs -0 sha256sum > SHA256SUMS
		sha256sum -c SHA256SUMS
	)
done

printf 'RAFCODEPHI_DEVELOPER_PACKAGE_BUILD=PASS_STATIC architectures=%s physical_android=TOKEN_VAZIO\n' "$ARCHITECTURES"
