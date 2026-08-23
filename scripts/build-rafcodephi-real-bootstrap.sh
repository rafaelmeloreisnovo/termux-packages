#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PACKAGE_NAME="${RAFCODEPHI_PACKAGE_NAME:-com.termux.rafacodephi}"
ARCHITECTURES="arm"
OUT_DIR="${RAFCODEPHI_BOOTSTRAP_OUT_DIR:-$ROOT/artifacts/rafcodephi-bootstrap}"
PROPERTIES="$ROOT/scripts/properties.sh"
LEGACY_PREFIX="/data/data/com.termux/files/usr"
TARGET_PREFIX="/data/data/${PACKAGE_NAME}/files/usr"
API_RECEIVER_COMPONENT="${PACKAGE_NAME}.api/com.termux.api.TermuxApiReceiver"

usage() {
    cat <<EOF
Usage: $0 [--architectures arm|aarch64|arm,aarch64] [--out DIR]

Builds a REAL RAFCODEPHI bootstrap from termux-packages source using
scripts/generate-bootstraps.sh --build. Bridge-only payloads are rejected.
The package-name override may be pre-materialized by CI before entering a
read-only builder mount; direct writable checkouts are patched transactionally.
EOF
}

while (($#)); do
    case "$1" in
        --architectures)
            ARCHITECTURES="${2:?missing architecture list}"
            shift 2
            ;;
        --out)
            OUT_DIR="${2:?missing output directory}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case ",$ARCHITECTURES," in
    *,arm,*|*,aarch64,*) ;;
    *) echo "Only arm and aarch64 are supported by this RAFCODEPHI bootstrap builder." >&2; exit 2 ;;
esac
if [[ "$ARCHITECTURES" == *x86* ]]; then
    echo "x86/x86_64 are intentionally not part of the first RAFCODEPHI real-bootstrap gate." >&2
    exit 2
fi

for cmd in python3 unzip zip file strings grep sed; do
    command -v "$cmd" >/dev/null || { echo "missing required command: $cmd" >&2; exit 127; }
done
[[ -f "$PROPERTIES" ]] || { echo "missing $PROPERTIES" >&2; exit 2; }
[[ -x "$ROOT/scripts/generate-bootstraps.sh" ]] || chmod +x "$ROOT/scripts/generate-bootstraps.sh" 2>/dev/null || true

validation_root="$(mktemp -d "${TMPDIR:-/tmp}/rafcodephi-bootstrap-validate.XXXXXX")"
backup=""
modified_properties=false
restore() {
    if [[ "$modified_properties" == true && -n "$backup" && -w "$PROPERTIES" ]]; then
        cp "$backup" "$PROPERTIES" || true
    fi
    [[ -z "$backup" ]] || rm -f "$backup"
    rm -rf "$validation_root"
}
trap restore EXIT INT TERM

target_assignment="TERMUX_APP__PACKAGE_NAME=\"${PACKAGE_NAME}\""
default_assignment='TERMUX_APP__PACKAGE_NAME="com.termux"'
if grep -Fxq "$target_assignment" "$PROPERTIES"; then
    echo "RAFCODEPHI package override already materialized; treating source tree as read-only."
elif grep -Fxq "$default_assignment" "$PROPERTIES"; then
    [[ -w "$PROPERTIES" ]] || {
        echo "properties.sh is read-only and RAFCODEPHI package override was not pre-materialized" >&2
        exit 1
    }
    backup="$(mktemp "${TMPDIR:-/tmp}/rafcodephi-properties.XXXXXX")"
    cp "$PROPERTIES" "$backup"
    python3 - "$PROPERTIES" "$PACKAGE_NAME" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
package = sys.argv[2]
text = path.read_text(encoding="utf-8")
old = 'TERMUX_APP__PACKAGE_NAME="com.termux"'
new = f'TERMUX_APP__PACKAGE_NAME="{package}"'
count = text.count(old)
if count != 1:
    raise SystemExit(f"expected exactly one canonical TERMUX_APP__PACKAGE_NAME assignment, found {count}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
PY
    modified_properties=true
else
    echo "properties.sh has neither canonical default nor expected RAFCODEPHI package assignment" >&2
    exit 1
fi

resolved="$(bash -c 'set -euo pipefail; source scripts/properties.sh; printf "%s\n%s\n" "$TERMUX_APP__PACKAGE_NAME" "$TERMUX__PREFIX"')"
resolved_package="$(printf '%s\n' "$resolved" | sed -n '1p')"
resolved_prefix="$(printf '%s\n' "$resolved" | sed -n '2p')"
[[ "$resolved_package" == "$PACKAGE_NAME" ]] || { echo "package override failed: $resolved_package" >&2; exit 1; }
[[ "$resolved_prefix" == "$TARGET_PREFIX" ]] || { echo "prefix override failed: $resolved_prefix != $TARGET_PREFIX" >&2; exit 1; }

echo "RAFCODEPHI source-build package=$resolved_package prefix=$resolved_prefix arch=$ARCHITECTURES"

rm -f bootstrap-arm.zip bootstrap-aarch64.zip
./scripts/generate-bootstraps.sh \
    --build \
    --architectures "$ARCHITECTURES" \
    --add busybox,proot,ca-certificates,termux-api

mkdir -p "$OUT_DIR"
manifest="$OUT_DIR/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
: > "$manifest"
printf 'schema=rafcodephi.real-bootstrap-sourcebuild/v1\n' >> "$manifest"
printf 'package_name=%s\n' "$PACKAGE_NAME" >> "$manifest"
printf 'prefix=%s\n' "$TARGET_PREFIX" >> "$manifest"
printf 'api_package=%s.api\n' "$PACKAGE_NAME" >> "$manifest"
printf 'api_receiver_component=%s\n' "$API_RECEIVER_COMPONENT" >> "$manifest"
printf 'api_access_control=SIGNATURE_PERMISSION_NO_SHARED_UID\n' >> "$manifest"
printf 'builder=termux-packages/scripts/generate-bootstraps.sh --build\n' >> "$manifest"
printf 'bridge_allowed=false\nlegacy_prefix_allowed=false\n' >> "$manifest"
printf 'termux_api_cli=EMBEDDED\n' >> "$manifest"
printf 'package_repo_runtime_state=BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED\n' >> "$manifest"
printf 'apt_update_guard=RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED\n' >> "$manifest"

IFS=',' read -r -a arch_list <<< "$ARCHITECTURES"
for arch in "${arch_list[@]}"; do
    [[ "$arch" == "arm" || "$arch" == "aarch64" ]] || { echo "unsupported arch: $arch" >&2; exit 2; }
    zip_path="$ROOT/bootstrap-${arch}.zip"
    [[ -s "$zip_path" ]] || { echo "missing generated $zip_path" >&2; exit 1; }

    # Seal the upstream-generated bootstrap with app-side evidence metadata before
    # publication. Standard Termux symlinks in SYMLINKS.txt count as installed
    # entries; they are not materialized inside the archive.
    python3 - "$zip_path" "$PACKAGE_NAME" "$TARGET_PREFIX" "$arch" "$API_RECEIVER_COMPONENT" <<'PY'
import json
import os
import sys
import zipfile
from pathlib import Path

zip_path = Path(sys.argv[1])
package_name = sys.argv[2]
prefix = sys.argv[3]
arch = sys.argv[4]
api_receiver_component = sys.argv[5]
required = [
    "BOOTSTRAP_INFO",
    "SYMLINKS.txt",
    "bin/sh",
    "bin/pkg",
    "bin/apt",
    "bin/apt-get",
    "bin/dpkg",
    "bin/bash",
    "bin/busybox",
    "bin/proot",
    "bin/termux-battery-status",
    "bin/termux-sensor",
    "libexec/termux-api",
    "libexec/termux-api-broadcast",
    "etc/apt/sources.list.d/termux.sources",
    "etc/apt/apt.conf.d/00rafcodephi-repository-block",
    "var/lib/dpkg/status",
]
apt_source_path = "etc/apt/sources.list.d/termux.sources"
apt_block_path = "etc/apt/apt.conf.d/00rafcodephi-repository-block"
apt_source_payload = (
    "# RAFCODEPHI_PACKAGE_REPOSITORY=BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED\n"
    "Enabled: no\n"
    "Types: deb\n"
    "URIs: https://packages.rafcodephi.invalid/termux\n"
    "Suites: stable\n"
    "Components: main\n"
    f"Signed-By: {prefix}/etc/apt/trusted.gpg.d/termux-packages.gpg\n"
).encode("utf-8")
apt_block_payload = (
    'APT::Update::Pre-Invoke { "echo RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED >&2; exit 100"; };\n'
).encode("utf-8")
with zipfile.ZipFile(zip_path, "r") as zf:
    names = set(zf.namelist())
    if "SYMLINKS.txt" not in names:
        raise SystemExit("cannot seal profile; SYMLINKS.txt missing")
    symlink_text = zf.read("SYMLINKS.txt").decode("utf-8")
if apt_source_path not in names:
    raise SystemExit(f"cannot seal profile; modern apt source missing: {apt_source_path}")
if "BOOTSTRAP_PROFILE.json" in names or "BOOTSTRAP_INFO" in names or apt_block_path in names:
    raise SystemExit("refusing to overwrite pre-existing RAFCODEPHI bootstrap metadata")
symlink_destinations = set()
for number, line in enumerate(symlink_text.splitlines(), 1):
    if not line:
        continue
    parts = line.split("←")
    if len(parts) != 2 or not parts[0] or not parts[1]:
        raise SystemExit(f"malformed SYMLINKS.txt line {number}: {line!r}")
    target, link = parts
    if link.startswith("/") or ".." in link or "\\" in link:
        raise SystemExit(f"unsafe symlink destination line {number}: {link!r}")
    symlink_destinations.add(link)
available = names | symlink_destinations | {
    "BOOTSTRAP_INFO",
    "BOOTSTRAP_PROFILE.json",
    apt_block_path,
}
missing = [name for name in required if name not in available]
if missing:
    raise SystemExit("cannot seal profile; missing installed entries: " + ",".join(missing))
profile = {
    "schema": "rafcodephi-bootstrap-profile/v1",
    "profile": "real-pkg",
    "package_layer": "real-pkg",
    "source": "termux-packages-sourcebuild",
    "package_name": package_name,
    "prefix": prefix,
    "arch": arch,
    "api_package": package_name + ".api",
    "api_receiver_component": api_receiver_component,
    "api_access_control": "SIGNATURE_PERMISSION_NO_SHARED_UID",
    "required_entries": required,
    "legacy_prefix_forbidden": True,
    "bridge_markers_forbidden": True,
    "runtime_materialized": False,
    "claim_allowed": False,
    "release_allowed": False,
    "device_validation": "TOKEN_VAZIO",
    "real_pkg_relocation_claim_allowed": False,
    "package_repo_runtime_state": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
    "apt_update_guard": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
}
bootstrap_info = {
    "TERMUX_PACKAGE_NAME": package_name,
    "TERMUX_ARCH": arch,
    "RAFCODEPHI_BOOTSTRAP_PROFILE": "real-pkg",
    "RAFCODEPHI_PACKAGE_LAYER": "real-pkg",
    "RAFCODEPHI_DEVICE_VALIDATION": "TOKEN_VAZIO",
    "RAFCODEPHI_CLAIM_ALLOWED": "0",
    "RAFCODEPHI_RUNTIME_MATERIALIZED": "0",
    "BOOTSTRAP_FULLENGINE_READY": "0",
    "BOOTSTRAP_PKG_REAL": "1",
    "BOOTSTRAP_APT_REAL": "1",
    "BOOTSTRAP_DPKG_REAL": "1",
    "BOOTSTRAP_TERMUX_API_CLI": "1",
    "RAFCODEPHI_API_PACKAGE": package_name + ".api",
    "RAFCODEPHI_API_RECEIVER_COMPONENT": api_receiver_component,
    "RAFCODEPHI_API_ACCESS_CONTROL": "SIGNATURE_PERMISSION_NO_SHARED_UID",
    "RAFCODEPHI_APT_UPDATE_GUARD": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
}
profile_payload = (json.dumps(profile, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
info_payload = "".join(f"{key}={bootstrap_info[key]}\n" for key in sorted(bootstrap_info)).encode("utf-8")
tmp_path = zip_path.with_name(zip_path.name + ".rafcodephi.tmp")
try:
    with zipfile.ZipFile(zip_path, "r") as source, zipfile.ZipFile(
        tmp_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as target:
        for source_info in source.infolist():
            payload = source.read(source_info.filename)
            if source_info.filename == apt_source_path:
                payload = apt_source_payload
            target.writestr(source_info, payload)
        for name, payload in (
            (apt_block_path, apt_block_payload),
            ("BOOTSTRAP_INFO", info_payload),
            ("BOOTSTRAP_PROFILE.json", profile_payload),
        ):
            info = zipfile.ZipInfo(name)
            info.date_time = (1980, 1, 1, 0, 0, 0)
            info.external_attr = 0o100600 << 16
            target.writestr(info, payload)
    os.replace(tmp_path, zip_path)
finally:
    if tmp_path.exists():
        tmp_path.unlink()
PY

    extract="$validation_root/$arch"
    mkdir -p "$extract"
    unzip -q "$zip_path" -d "$extract"

    for required in BOOTSTRAP_INFO BOOTSTRAP_PROFILE.json SYMLINKS.txt bin/apt bin/apt-get bin/dpkg bin/bash bin/pkg bin/busybox bin/proot bin/termux-battery-status bin/termux-sensor libexec/termux-api-broadcast etc/apt/sources.list.d/termux.sources etc/apt/apt.conf.d/00rafcodephi-repository-block var/lib/dpkg/status; do
        [[ -f "$extract/$required" ]] || { echo "$arch missing real bootstrap archive target: $required" >&2; exit 1; }
    done
    grep -Eq '^Package: termux-api$' "$extract/var/lib/dpkg/status" || {
        echo "$arch dpkg status does not contain the embedded termux-api package" >&2
        exit 1
    }

    python3 - "$extract/BOOTSTRAP_PROFILE.json" "$extract/BOOTSTRAP_INFO" "$PACKAGE_NAME" "$TARGET_PREFIX" "$arch" "$API_RECEIVER_COMPONENT" "$zip_path" <<'PY'
import json
import sys
import zipfile
from pathlib import Path
p = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
info = {}
for line in Path(sys.argv[2]).read_text(encoding="utf-8").splitlines():
    if "=" in line:
        key, value = line.split("=", 1)
        info[key] = value
expected = {
    "schema": "rafcodephi-bootstrap-profile/v1",
    "profile": "real-pkg",
    "package_layer": "real-pkg",
    "package_name": sys.argv[3],
    "prefix": sys.argv[4],
    "arch": sys.argv[5],
    "api_package": sys.argv[3] + ".api",
    "api_receiver_component": sys.argv[6],
    "api_access_control": "SIGNATURE_PERMISSION_NO_SHARED_UID",
    "runtime_materialized": False,
    "claim_allowed": False,
    "release_allowed": False,
    "device_validation": "TOKEN_VAZIO",
    "real_pkg_relocation_claim_allowed": False,
    "package_repo_runtime_state": "BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED",
    "apt_update_guard": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
}
for key, value in expected.items():
    if p.get(key) != value:
        raise SystemExit(f"profile contract mismatch {key}: {p.get(key)!r} != {value!r}")
for key, value in {
    "TERMUX_PACKAGE_NAME": sys.argv[3],
    "TERMUX_ARCH": sys.argv[5],
    "RAFCODEPHI_BOOTSTRAP_PROFILE": "real-pkg",
    "RAFCODEPHI_PACKAGE_LAYER": "real-pkg",
    "RAFCODEPHI_DEVICE_VALIDATION": "TOKEN_VAZIO",
    "RAFCODEPHI_CLAIM_ALLOWED": "0",
    "RAFCODEPHI_RUNTIME_MATERIALIZED": "0",
    "BOOTSTRAP_FULLENGINE_READY": "0",
    "BOOTSTRAP_PKG_REAL": "1",
    "BOOTSTRAP_APT_REAL": "1",
    "BOOTSTRAP_DPKG_REAL": "1",
    "BOOTSTRAP_TERMUX_API_CLI": "1",
    "RAFCODEPHI_API_PACKAGE": sys.argv[3] + ".api",
    "RAFCODEPHI_API_RECEIVER_COMPONENT": sys.argv[6],
    "RAFCODEPHI_API_ACCESS_CONTROL": "SIGNATURE_PERMISSION_NO_SHARED_UID",
    "RAFCODEPHI_APT_UPDATE_GUARD": "RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED",
}.items():
    if info.get(key) != value:
        raise SystemExit(f"BOOTSTRAP_INFO mismatch {key}: {info.get(key)!r} != {value!r}")
required = p.get("required_entries")
if not isinstance(required, list) or not required:
    raise SystemExit("profile required_entries empty")
with zipfile.ZipFile(sys.argv[7], "r") as zf:
    names = set(zf.namelist())
    symlinks = zf.read("SYMLINKS.txt").decode("utf-8").splitlines()
links = set()
for line in symlinks:
    if not line:
        continue
    parts = line.split("←")
    if len(parts) != 2:
        raise SystemExit(f"malformed symlink line: {line!r}")
    links.add(parts[1])
available = names | links
missing = [name for name in required if name not in available]
if missing:
    raise SystemExit("profile required entries unresolved by archive/symlinks: " + ",".join(missing))
if "bin/sh" not in available:
    raise SystemExit("installed bin/sh is not represented by archive or symlink")
PY

    for elf in bin/apt bin/apt-get bin/dpkg bin/bash bin/busybox bin/proot libexec/termux-api-broadcast; do
        desc="$(file -b "$extract/$elf")"
        case "$desc" in
            *ELF*) ;;
            *) echo "$arch $elf is not ELF: $desc" >&2; exit 1 ;;
        esac
        if grep -aFq "$LEGACY_PREFIX" "$extract/$elf"; then
            echo "$arch $elf embeds forbidden legacy prefix $LEGACY_PREFIX" >&2
            exit 1
        fi
    done

    api_target="$(sed -n 's#^termux-api-broadcast←libexec/termux-api$#termux-api-broadcast#p' "$extract/SYMLINKS.txt")"
    [[ "$api_target" == "termux-api-broadcast" ]] || {
        echo "$arch termux-api compatibility symlink is missing from SYMLINKS.txt" >&2
        exit 1
    }

    if ! grep -aFq "$API_RECEIVER_COMPONENT" "$extract/libexec/termux-api-broadcast"; then
        echo "$arch termux-api client does not target the RAFCODEPHI API receiver" >&2
        exit 1
    fi
    if grep -aFq 'com.termux/com.termux.app.TermuxService' "$extract/libexec/termux-api-broadcast" || \
       grep -aFq 'com.termux.service_api' "$extract/libexec/termux-api-broadcast"; then
        echo "$arch termux-api client contains the removed service stub route" >&2
        exit 1
    fi

    if grep -aFq 'RAFCODEPHI pkg bridge' "$extract/bin/pkg" || \
       grep -aFq 'real apt/apt-get backend is not installed yet' "$extract/bin/pkg"; then
        echo "$arch pkg is a bridge, not a real package-manager frontend" >&2
        exit 1
    fi

    if grep -R -aFq "$LEGACY_PREFIX" "$extract/etc/apt" "$extract/bin/pkg" "$extract/bin/termux-setup-package-manager" 2>/dev/null; then
        echo "$arch apt/pkg tooling still contains legacy prefix" >&2
        exit 1
    fi

    if ! grep -Fxq '# RAFCODEPHI_PACKAGE_REPOSITORY=BLOCKED_CUSTOM_REPOSITORY_NOT_PUBLISHED' "$extract/etc/apt/sources.list.d/termux.sources" || \
       ! grep -Fxq 'Enabled: no' "$extract/etc/apt/sources.list.d/termux.sources" || \
       grep -Fq 'termux.net' "$extract/etc/apt/sources.list.d/termux.sources"; then
        echo "$arch apt repository is not safely blocked for the custom-prefix payload" >&2
        exit 1
    fi
    if ! grep -Fq 'RAFCODEPHI_PACKAGE_REPOSITORY_NOT_PUBLISHED' "$extract/etc/apt/apt.conf.d/00rafcodephi-repository-block"; then
        echo "$arch apt update fail-closed hook is missing" >&2
        exit 1
    fi

    out="$OUT_DIR/rafcodephi-bootstrap-${arch}.zip"
    cp "$zip_path" "$out"
    bytes="$(wc -c < "$out" | tr -d ' ')"
    printf 'artifact_%s=%s\nbytes_%s=%s\n' "$arch" "$out" "$arch" "$bytes" >> "$manifest"
    echo "PASS real bootstrap arch=$arch bytes=$bytes"
done

printf 'claim_allowed_device_runtime=false\ndevice_runtime_proof=TOKEN_VAZIO\n' >> "$manifest"
echo "REAL_BOOTSTRAP_SOURCEBUILD=PASS manifest=$manifest"
