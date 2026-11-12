#!/usr/bin/env bash
# Scoped source-link compatibility, not binary editing or runtime validation.
# Source from an affected package's termux_step_pre_configure.
rafcodephi_apply_memfd29_link() {
    if [[ "${TERMUX_APP__PACKAGE_NAME:-}" != 'com.termux.rafacodephi' ||
          "${TERMUX_PKG_API_LEVEL:-}" != '29' ||
          "${TERMUX_ARCH:-}" != 'aarch64' ]]; then
        return 0
    fi

    local src="${TERMUX_SCRIPTDIR:?}/scripts/ci/rafcodephi_memfd29_compat.c"
    local obj="${TERMUX_PKG_TMPDIR:?}/rafcodephi_memfd29_compat.o"
    if [[ ! -s "$src" || ! -d "${TERMUX_PKG_TMPDIR}" ]]; then
        printf 'RAFCODEPHI_MEMFD29=BLOCKED reason=source_or_build_dir_missing\n' >&2
        return 1
    fi
    if [[ -z "${CC:-}" ]] || ! command -v "$CC" >/dev/null 2>&1; then
        printf 'RAFCODEPHI_MEMFD29=BLOCKED reason=target_compiler_unavailable\n' >&2
        return 1
    fi
    if ! "$CC" -fPIC -O2 -c "$src" -o "$obj"; then
        printf 'RAFCODEPHI_MEMFD29=BLOCKED reason=target_object_compile_failed\n' >&2
        return 1
    fi
    # The wrapper is resolved within each affected library, so no additional
    # runtime .so is introduced and no Android 30 libc symbol is required.
    LDFLAGS="${LDFLAGS:-} -Wl,--wrap=memfd_create $obj"
    printf 'RAFCODEPHI_MEMFD29=SOURCE_LINK_ADAPTER arch=aarch64 api=29 package=%s\n' "${TERMUX_PKG_NAME:-unknown}"
}
