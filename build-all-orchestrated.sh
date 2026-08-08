#!/bin/bash
# build-all-orchestrated.sh - Orchestrated build system with toroidal layer scheduling
# Implements Phase 9.13 of Sistema Núcleo Autoral

set -e -u -o pipefail

TERMUX_SCRIPTDIR=$(cd "$(realpath "$(dirname "$0")")"; pwd)

# Store pid of current process in a file for docker__run_docker_exec_trap
source "$TERMUX_SCRIPTDIR/scripts/utils/docker/docker.sh" 2>/dev/null || true
docker__create_docker_exec_pid_file 2>/dev/null || true

if [ "$(uname -o)" = "Android" ] || [ -e "/system/bin/app_process" ]; then
    echo "On-device execution of this script is not supported."
    exit 1
fi

# Read settings from .termuxrc if existing
test -f "$HOME"/.termuxrc && . "$HOME"/.termuxrc
: ${TERMUX_TOPDIR:="$HOME/.termux-build"}
: ${TERMUX_ARCH:="aarch64"}
: ${TERMUX_FORMAT:="debian"}
: ${TERMUX_DEBUG_BUILD:=""}
: ${TERMUX_INSTALL_DEPS:="-s"}

_show_usage() {
    echo "Usage: ./build-all-orchestrated.sh [-a ARCH] [-d] [-i] [-o DIR] [-f FORMAT]"
    echo "Build all packages using orchestrated system with toroidal layer scheduling."
    echo "  -a The architecture to build for: aarch64(default), arm, i686, x86_64 or all."
    echo "  -d Build with debug symbols."
    echo "  -i Build dependencies."
    echo "  -o Specify deb directory. Default: debs/."
    echo "  -f Specify format pkg: debian(default) or pacman."
    exit 1
}

while getopts :a:hdio:f: option; do
case "$option" in
    a) TERMUX_ARCH="$OPTARG";;
    d) TERMUX_DEBUG_BUILD='-d';;
    i) TERMUX_INSTALL_DEPS='-i';;
    o) TERMUX_OUTPUT_DIR="$(realpath -m "$OPTARG")";;
    f) TERMUX_FORMAT="$OPTARG";;
    h) _show_usage;;
    *) _show_usage >&2 ;;
esac
done
shift $((OPTIND-1))
if [ "$#" -ne 0 ]; then _show_usage; fi

case "$TERMUX_ARCH" in
    all|aarch64|arm|i686|x86_64);;
    *) echo "ERROR: Invalid arch '$TERMUX_ARCH'" 1>&2; exit 1;;
esac

case "$TERMUX_FORMAT" in
    debian|pacman);;
    *) echo "ERROR: Invalid format '$TERMUX_FORMAT'" 1>&2; exit 1;;
esac

BUILDSCRIPT=$(dirname "$0")/build-package.sh
ORCHESTRATOR_BIN="$TERMUX_SCRIPTDIR/core/orchestrator-build-main"
BUILDALL_DIR=$TERMUX_TOPDIR/_buildall-orchestrated-$TERMUX_ARCH

if [ ! -f "$ORCHESTRATOR_BIN" ]; then
    echo "ERROR: Orchestrator binary not found at $ORCHESTRATOR_BIN"
    echo "Please compile the project with: cd core && make orchestrator-build-main"
    exit 1
fi

if [ ! -f "$BUILDSCRIPT" ]; then
    echo "ERROR: Build script not found at $BUILDSCRIPT"
    exit 1
fi

mkdir -p "$BUILDALL_DIR"

echo "=================================================================================="
echo "  TERMUX ORCHESTRATED BUILD SYSTEM"
echo "  Phase 9.13: Toroidal Layer Scheduling & Zero-Abstraction Integration"
echo "=================================================================================="
echo ""
echo "Configuration:"
echo "  Architecture: $TERMUX_ARCH"
echo "  Format: $TERMUX_FORMAT"
echo "  Debug Build: ${TERMUX_DEBUG_BUILD:-no}"
echo "  Install Deps: ${TERMUX_INSTALL_DEPS:-no}"
echo "  Build Script: $BUILDSCRIPT"
echo "  Orchestrator: $ORCHESTRATOR_BIN"
if [ -n "${TERMUX_OUTPUT_DIR:-}" ]; then
    echo "  Output Dir: $TERMUX_OUTPUT_DIR"
fi
echo "  Build Log Dir: $BUILDALL_DIR"
echo ""

# Build orchestrator arguments
ORCH_ARGS="-a $TERMUX_ARCH"
ORCH_ARGS="$ORCH_ARGS -f $TERMUX_FORMAT"
if [ -n "${TERMUX_DEBUG_BUILD}" ]; then
    ORCH_ARGS="$ORCH_ARGS -d"
fi
if [ -n "${TERMUX_INSTALL_DEPS}" ]; then
    ORCH_ARGS="$ORCH_ARGS -i"
fi
if [ -n "${TERMUX_OUTPUT_DIR:-}" ]; then
    ORCH_ARGS="$ORCH_ARGS -o $TERMUX_OUTPUT_DIR"
fi
ORCH_ARGS="$ORCH_ARGS -b $BUILDSCRIPT"

echo "Starting orchestrated build..."
echo ""

exec &> >(tee -a "$BUILDALL_DIR"/orchestrated-build.log)
trap 'echo "ERROR: See $BUILDALL_DIR/orchestrated-build.log" >&2' ERR

# Execute the orchestrator
"$ORCHESTRATOR_BIN" $ORCH_ARGS

ORCHESTRATOR_EXIT=$?

echo ""
echo "=================================================================================="
if [ $ORCHESTRATOR_EXIT -eq 0 ]; then
    echo "✓ ORCHESTRATED BUILD COMPLETED SUCCESSFULLY"
else
    echo "✗ ORCHESTRATED BUILD FAILED"
fi
echo "=================================================================================="
echo ""
echo "Build logs available in: $BUILDALL_DIR"
echo ""

exit $ORCHESTRATOR_EXIT
