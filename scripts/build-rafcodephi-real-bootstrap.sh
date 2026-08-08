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

usage() {
    cat <<EOF
Usage: $0 [--architectures arm|aarch64|arm,aarch64] [--out DIR]

Builds a REAL RAFCODEPHI bootstrap from termux-packages source using
scripts/generate-bootstraps.sh --build. Bridge-only payloads are rejected.
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

for cmd in python3 unzip zip file strings sha256sum grep sed; do
    command -v "$cmd" >/dev/null || { echo "missing required command: $cmd" >&2; exit 127; }
done
[[ -f "$PROPERTIES" ]] || { echo "missing $PROPERTIES" >&2; exit 2; }
[[ -x "$ROOT/scripts/generate-bootstraps.sh" ]] || chmod +x "$ROOT/scripts/generate-bootstraps.sh"

backup="$(mktemp "${TMPDIR:-/tmp}/rafcodephi-properties.XXXXXX")"
validation_root="$(mktemp -d "${TMPDIR:-/tmp}/rafcodephi-bootstrap-validate.XXXXXX")"
cp "$PROPERTIES" "$backup"
restore() {
    cp "$backup" "$PROPERTIES" || true
    rm -f "$backup"
    rm -rf "$validation_root"
}
trap restore EXIT INT TERM

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
    --add busybox,proot,ca-certificates

mkdir -p "$OUT_DIR"
manifest="$OUT_DIR/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
: > "$manifest"
printf 'schema=rafcodephi.real-bootstrap-sourcebuild/v1\n' >> "$manifest"
printf 'package_name=%s\n' "$PACKAGE_NAME" >> "$manifest"
printf 'prefix=%s\n' "$TARGET_PREFIX" >> "$manifest"
printf 'builder=termux-packages/scripts/generate-bootstraps.sh --build\n' >> "$manifest"
printf 'bridge_allowed=false\nlegacy_prefix_allowed=false\n' >> "$manifest"

IFS=',' read -r -a arch_list <<< "$ARCHITECTURES"
for arch in "${arch_list[@]}"; do
    [[ "$arch" == "arm" || "$arch" == "aarch64" ]] || { echo "unsupported arch: $arch" >&2; exit 2; }
    zip_path="$ROOT/bootstrap-${arch}.zip"
    [[ -s "$zip_path" ]] || { echo "missing generated $zip_path" >&2; exit 1; }

    extract="$validation_root/$arch"
    mkdir -p "$extract"
    unzip -q "$zip_path" -d "$extract"

    for required in bin/apt bin/apt-get bin/dpkg bin/bash bin/pkg bin/busybox bin/proot; do
        [[ -f "$extract/$required" ]] || { echo "$arch missing real bootstrap target: $required" >&2; exit 1; }
    done

    for elf in bin/apt bin/apt-get bin/dpkg bin/bash bin/busybox bin/proot; do
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

    if grep -aFq 'RAFCODEPHI pkg bridge' "$extract/bin/pkg" || \
       grep -aFq 'real apt/apt-get backend is not installed yet' "$extract/bin/pkg"; then
        echo "$arch pkg is a bridge, not a real package-manager frontend" >&2
        exit 1
    fi

    if grep -R -aFq "$LEGACY_PREFIX" "$extract/etc/apt" "$extract/bin/pkg" 2>/dev/null; then
        echo "$arch apt/pkg configuration still contains legacy prefix" >&2
        exit 1
    fi

    if ! grep -R -aEq '(^|[[:space:]])deb[[:space:]]+https?://' "$extract/etc/apt" 2>/dev/null; then
        echo "$arch bootstrap has no HTTP(S) apt repository configuration" >&2
        exit 1
    fi

    out="$OUT_DIR/rafcodephi-bootstrap-${arch}.zip"
    cp "$zip_path" "$out"
    digest="$(sha256sum "$out" | awk '{print $1}')"
    bytes="$(wc -c < "$out" | tr -d ' ')"
    printf 'artifact_%s=%s\nsha256_%s=%s\nbytes_%s=%s\n' "$arch" "$out" "$arch" "$digest" "$arch" "$bytes" >> "$manifest"
    echo "PASS real bootstrap arch=$arch sha256=$digest bytes=$bytes"
done

printf 'claim_allowed_device_runtime=false\ndevice_runtime_proof=TOKEN_VAZIO\n' >> "$manifest"
echo "REAL_BOOTSTRAP_SOURCEBUILD=PASS manifest=$manifest"
