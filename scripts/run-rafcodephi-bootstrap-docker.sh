#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

HOST_OUT="${RAFCODEPHI_BOOTSTRAP_HOST_OUT_DIR:-$ROOT/artifacts/rafcodephi-bootstrap}"
CONTAINER_OUT="${RAFCODEPHI_BOOTSTRAP_CONTAINER_OUT_DIR:-/tmp/rafcodephi-bootstrap-out}"
CONTAINER="${CONTAINER_NAME:-termux-package-builder}"

if (($# == 0)); then
    echo "Usage: $0 COMMAND [ARGS...]" >&2
    echo "Runs a RAFCODEPHI bootstrap command in the canonical Termux builder," >&2
    echo "forcing output to a container-local writable directory and copying" >&2
    echo "the resulting evidence back to the host checkout." >&2
    exit 2
fi

test -x scripts/run-docker.sh || chmod +x scripts/run-docker.sh

docker_cmd=(docker)
if [[ -n "${TERMUX_DOCKER_USE_SUDO:-}" ]]; then
    docker_cmd=(sudo docker)
fi

rm -rf "$HOST_OUT"
mkdir -p "$HOST_OUT"

# Never inherit stale evidence from a previous command in the persistent builder.
./scripts/run-docker.sh sh -c 'rm -rf "$1" && mkdir -p "$1"' _ "$CONTAINER_OUT"

set +e
./scripts/run-docker.sh env \
    RAFCODEPHI_BOOTSTRAP_OUT_DIR="$CONTAINER_OUT" \
    "$@"
build_status=$?
set -e

copy_status=0
if "${docker_cmd[@]}" exec "$CONTAINER" test -d "$CONTAINER_OUT"; then
    "${docker_cmd[@]}" cp "$CONTAINER:$CONTAINER_OUT/." "$HOST_OUT/" || copy_status=$?
    if [[ "${docker_cmd[0]}" == sudo ]]; then
        sudo chown -R "$(id -u):$(id -g)" "$HOST_OUT" 2>/dev/null || true
    fi
else
    copy_status=1
fi

if ((build_status != 0)); then
    echo "RAFCODEPHI_BOOTSTRAP_DOCKER=BLOCKED build_exit=$build_status copy_exit=$copy_status" >&2
    exit "$build_status"
fi
if ((copy_status != 0)); then
    echo "RAFCODEPHI_BOOTSTRAP_DOCKER=BLOCKED build_exit=0 copy_exit=$copy_status" >&2
    exit "$copy_status"
fi

manifest="$HOST_OUT/RAFCODEPHI_REAL_BOOTSTRAP_MANIFEST.txt"
test -s "$manifest" || {
    echo "RAFCODEPHI_BOOTSTRAP_DOCKER=BLOCKED reason=MANIFEST_NOT_COPIED" >&2
    exit 1
}

echo "RAFCODEPHI_BOOTSTRAP_DOCKER=PASS host_out=$HOST_OUT container_out=$CONTAINER_OUT"
