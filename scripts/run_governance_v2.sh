#!/usr/bin/env bash
# Single-entry governance V2 runner.
#
# Modes:
#   validate (default): validate registry/quarantine/contracts/runtime probes
#                       + pkg_metrics governance.
#   promote:            run validate, then require strict Reality V2 gate.
#
# A validate PASS does not mean promotion PASS. Promotion remains blocked while
# any P0 evidence axis is FAIL/BLOCKED/TOKEN_VAZIO or a classification conflict
# exists.

set -euo pipefail

MODE="${1:-validate}"
AUDIT_OUT="${AUDIT_OUT:-/tmp/reality_audit_v2.$$.json}"
ARCH_OUT="${ARCH_OUT:-/tmp/arch_runtime_probe.$$.json}"
KEEP_AUDIT="${KEEP_AUDIT:-0}"

cleanup() {
    if [ "$KEEP_AUDIT" != "1" ]; then
        rm -f "$AUDIT_OUT" "$ARCH_OUT"
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

for cmd in python3 make jq cc; do
    command -v "$cmd" >/dev/null 2>&1 || {
        echo "BLOCKED: required command missing: $cmd" >&2
        exit 1
    }
done

echo "[1/9] Reality V2 registry/unit tests"
python3 -m unittest core/tests/test_reality_audit_v2.py

echo "[2/9] Toy crypto quarantine gate"
python3 -m unittest core/tests/test_toy_crypto_quarantine.py

echo "[3/9] Strict pkg_metrics adversarial contract tests"
python3 -m unittest core/tests/test_contract_adversarial.py

echo "[4/9] Runtime architecture probe tests"
python3 -m unittest core/tests/test_arch_runtime_probe.py

echo "[5/9] Runtime architecture receipt"
python3 scripts/arch_runtime_probe.py --out "$ARCH_OUT"

echo "[6/9] Reality V2 report (non-promoting)"
python3 core/audit_reality_v2.py --out "$AUDIT_OUT"

echo "[7/9] Build governed metrics binaries"
make -C core metrics-producer contract-validate

echo "[8/9] pkg_metrics governance + adversarial baseline tests"
bash core/tests/test_governance.sh

echo "[9/9] Scope assertion"
echo "VALIDATION_GATE=PASS"
echo "STRICT_JSON_GATE=PASS"
echo "DUPLICATE_KEYS=REJECTED"
echo "SCOPE_ENFORCEMENT=PASS"
echo "TOY_CRYPTO_QUARANTINE=PASS"
echo "TOY_CRYPTO_PRODUCTION_ALLOWED=false"
echo "ARCH_RUNTIME_PROBE=OBSERVED_LIMITED"
echo "ARCH_NOMINAL_MATRIX_IS_RUNTIME_EVIDENCE=false"
echo "PRODUCT_READINESS=NOT_CLAIMED"
echo "DEVICE_RUNTIME=TOKEN_VAZIO_UNLESS_SEPARATE_RECEIPT"
echo "SECURITY=FAIL_FOR_TOY_CRYPTO_TOKEN_VAZIO_ELSEWHERE"
echo "audit_report=$AUDIT_OUT"
echo "arch_report=$ARCH_OUT"

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
