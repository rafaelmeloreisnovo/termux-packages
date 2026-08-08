#!/usr/bin/env bash
# Single-entry governance V2 runner.
#
# Modes:
#   validate (default): validate registry/tests + pkg_metrics governance.
#   promote:            run validate, then require strict Reality V2 gate.
#
# A validate PASS does not mean promotion PASS. Promotion remains blocked while
# any P0 evidence axis is FAIL/BLOCKED/TOKEN_VAZIO or a classification conflict
# exists.

set -euo pipefail

MODE="${1:-validate}"
AUDIT_OUT="${AUDIT_OUT:-/tmp/reality_audit_v2.$$.json}"
KEEP_AUDIT="${KEEP_AUDIT:-0}"

cleanup() {
    if [ "$KEEP_AUDIT" != "1" ]; then
        rm -f "$AUDIT_OUT"
    fi
}
trap cleanup EXIT

case "$MODE" in
    validate|promote) ;;
    *)
        echo "usage: $0 [validate|promote]" >&2
        exit 64
        ;;
esac

for cmd in python3 make jq; do
    command -v "$cmd" >/dev/null 2>&1 || {
        echo "BLOCKED: required command missing: $cmd" >&2
        exit 1
    }
done

echo "[1/5] Reality V2 registry/unit tests"
python3 -m unittest core/tests/test_reality_audit_v2.py

echo "[2/5] Reality V2 report (non-promoting)"
python3 core/audit_reality_v2.py --out "$AUDIT_OUT"

echo "[3/5] Build governed metrics binaries"
make -C core metrics-producer contract-validate

echo "[4/5] pkg_metrics governance + adversarial baseline tests"
bash core/tests/test_governance.sh

echo "[5/5] Scope assertion"
echo "VALIDATION_GATE=PASS"
echo "PRODUCT_READINESS=NOT_CLAIMED"
echo "DEVICE_RUNTIME=TOKEN_VAZIO_UNLESS_SEPARATE_RECEIPT"
echo "SECURITY=TOKEN_VAZIO_OR_FAIL_PER_MODULE_REGISTRY"
echo "audit_report=$AUDIT_OUT"

if [ "$MODE" = "promote" ]; then
    echo "[promotion] strict Reality V2 gate"
    if python3 core/audit_reality_v2.py --out "$AUDIT_OUT" --strict; then
        echo "PROMOTION_GATE=PASS"
    else
        rc=$?
        echo "PROMOTION_GATE=BLOCKED" >&2
        exit "$rc"
    fi
fi
