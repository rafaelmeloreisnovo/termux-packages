#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ROOTS_FILE="${RAFCODEPHI_CORE_ROOTS_FILE:-scripts/rafcodephi-core-packages.txt}"
OUT_ROOT="${RAFCODEPHI_CORE_OUT:-build/rafcodephi-core-v1}"
JOBS="${RAFCODEPHI_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"

mapfile -t ROOTS < <(sed -e 's/[[:space:]]*#.*$//' -e '/^[[:space:]]*$/d' "$ROOTS_FILE")
if [[ "${#ROOTS[@]}" -eq 0 ]]; then
  echo "RAFCODEPHI_CORE_ROOTS_EMPTY" >&2
  exit 2
fi

mkdir -p "$OUT_ROOT"

build_arch() {
  local arch="$1"
  local pkg_out="$OUT_ROOT/packages/$arch"
  mkdir -p "$pkg_out"

  printf 'RAFCODEPHI_SOURCE_BUILD_BEGIN arch=%s roots=%s\n' "$arch" "${#ROOTS[@]}"
  ./build-package-rafcodephi.sh \
    -a "$arch" \
    -j "$JOBS" \
    -o "$pkg_out" \
    "${ROOTS[@]}"

  local count
  count="$(find "$pkg_out" -maxdepth 1 -type f -name '*.deb' | wc -l | tr -d ' ')"
  if [[ "$count" -eq 0 ]]; then
    printf 'RAFCODEPHI_SOURCE_BUILD_NO_DEBS arch=%s\n' "$arch" >&2
    exit 3
  fi

  python3 scripts/emit-rafcodephi-local-repo.py \
    --arch "$arch" \
    --packages-dir "$pkg_out" \
    --output "$OUT_ROOT/handoff/$arch"

  printf 'RAFCODEPHI_SOURCE_BUILD_PASS arch=%s debs=%s claim_allowed_runtime=false\n' "$arch" "$count"
}

build_arch aarch64
build_arch arm

python3 scripts/emit-rafcodephi-matrix-receipt.py \
  --root "$OUT_ROOT/handoff" \
  --output "$OUT_ROOT/rafcodephi-core-matrix.v1.json"

printf 'RAFCODEPHI_SOURCE_CORE_MATRIX=BUILT_STRUCTURAL device_validation=TOKEN_VAZIO\n'
