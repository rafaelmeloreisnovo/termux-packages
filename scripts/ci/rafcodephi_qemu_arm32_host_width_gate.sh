#!/usr/bin/env bash
# RAFCODEPHI: early, evidence-producing guard for QEMU 10.2.1 x86_64-softmmu
# on 32-bit Android/ARM hosts. This does not certify a built artifact.
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo 'usage: rafcodephi_qemu_arm32_host_width_gate.sh EVIDENCE_DIR PACKAGE_IDENTITY' >&2
  exit 64
fi

evidence_dir="$1"
package_identity="$2"
case "$evidence_dir" in
  rafcodephi-qemu-arm32-target-proof|rafcodephi-qemu-arm32-evidence) ;;
  *) echo 'refusing unexpected evidence directory' >&2; exit 64 ;;
esac
case "$package_identity" in
  com.termux|com.termux.rafacodephi) ;;
  *) echo 'refusing unexpected package identity' >&2; exit 64 ;;
esac

recipe=packages/qemu-system-x86-64-headless/build.sh
test -f "$recipe"
mkdir -p "$evidence_dir"
source_commit="$(git rev-parse HEAD)"
recipe_sha256="$(sha256sum "$recipe" | awk '{print $1}')"
guard_sha256="$(sha256sum "$0" | awk '{print $1}')"

# Both targets are sourced from the pinned QEMU 10.2.1 code:
# meson.build: host_long_bits = cc.sizeof('void *') * 8 (line 3250);
# meson.build:3350-3364: skip accelerators when host_long_bits < TARGET_LONG_BITS.
# configs/targets/x86_64-softmmu.mak: TARGET_LONG_BITS=64.
# An ARM32 cross build has 32-bit host pointers. Neither NDK29 nor KVM
# changes this QEMU source constraint.
if grep -Fqx 'TERMUX_PKG_VERSION="1:10.2.1"' "$recipe" \
  && grep -Fq 'QEMU_TARGETS+="x86_64-softmmu,"' "$recipe" \
  && grep -Fq '"$TERMUX_ARCH" == "arm"' "$recipe"; then
  state=BLOCKED_QEMU_10_2_1_HOST_WIDTH
  stop_reason=QEMU_MESON_HOST32_TARGET64_REJECTS_ALL_ACCELERATORS
  exit_code=78
else
  state=BLOCKED_UNVERIFIED_RECIPE_CONTRACT
  stop_reason=SOURCE_CHANGED_REQUIRES_INDEPENDENT_REASSESSMENT
  exit_code=79
fi

cat > "$evidence_dir/receipt.txt" <<EOF
schema=rafcodephi.qemu-arm32-host-width-gate/v1
source_repository=rafaelmeloreisnovo/termux-packages
source_commit=${source_commit}
recipe_path=${recipe}
recipe_sha256=${recipe_sha256}
guard_sha256=${guard_sha256}
package_identity=${package_identity}
host_abi=arm
host_pointer_bits_inferred=32
guest_system_target=x86_64-softmmu
guest_target_long_bits=64
upstream_qemu_tag=v10.2.1
upstream_meson=https://github.com/qemu/qemu/blob/v10.2.1/meson.build
gate_state=${state}
stop_reason=${stop_reason}
build_state=NOT_RUN
package_sha256=TOKEN_VAZIO
binary_sha256=TOKEN_VAZIO
physical_android_runtime=TOKEN_VAZIO
claim_allowed=false
exit_code=${exit_code}
EOF
echo "::error title=QEMU ARM32 x86_64 target blocked::${stop_reason}; receipt in ${evidence_dir}/receipt.txt"
exit "$exit_code"
