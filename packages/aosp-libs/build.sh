TERMUX_PKG_HOMEPAGE=https://source.android.com/
TERMUX_PKG_DESCRIPTION="bionic libc, libicuuc, liblzma, zlib, and boringssl for package builder and termux-docker"
TERMUX_PKG_LICENSE="BSD 3-Clause, Apache-2.0, ZLIB, Public Domain, BSD 2-Clause, OpenSSL, MirOS, BSD"
TERMUX_PKG_LICENSE_FILE="
bionic/libc/NOTICE
external/zlib/LICENSE
external/lzma/NOTICE
external/icu/LICENSE
external/boringssl/NOTICE
external/mksh/NOTICE
external/toybox/LICENSE
external/iputils/NOTICE
"
TERMUX_PKG_MAINTAINER="@termux"
# Debian package versions cannot contain the underscore used by AOSP manifest tags.
# Keep package version and source ref separate so both contracts remain valid.
TERMUX_PKG_VERSION="16.0.0.r4"
TERMUX_PKG_AUTO_UPDATE=false
TERMUX_PKG_BUILD_IN_SRC=true
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_UNDEF_SYMBOLS_FILES="all"
TERMUX_PKG_DEPENDS="resolv-conf"
TERMUX_PKG_BREAKS="bionic-host"
TERMUX_PKG_REPLACES="bionic-host"
TERMUX_PKG_ON_DEVICE_BUILD_NOT_SUPPORTED=true

_RAFCODEPHI_AOSP_MANIFEST_REF="android-16.0.0_r4"

termux_step_get_source() {
	if $TERMUX_ON_DEVICE_BUILD; then
		termux_error_exit "Package '$TERMUX_PKG_NAME' is not safe for on-device builds."
	fi

	termux_download https://storage.googleapis.com/git-repo-downloads/repo \
		"${TERMUX_PKG_CACHEDIR}/repo" SKIP_CHECKSUM
	chmod +x "${TERMUX_PKG_CACHEDIR}/repo"

	rm -rf "${TERMUX_PKG_SRCDIR}"
	mkdir -p "${TERMUX_PKG_SRCDIR}"
	cd "${TERMUX_PKG_SRCDIR}" || termux_error_exit "Couldn't enter source code directory: ${TERMUX_PKG_SRCDIR}"

	[[ "$(git config --get user.name)" != '' ]] || git config --global user.name "Termux Github Actions"
	[[ "$(git config --get user.email)" != '' ]] || git config --global user.email "contact@termux.dev"

	"${TERMUX_PKG_CACHEDIR}/repo" init \
		--partial-clone \
		--no-use-superproject \
		-b "${_RAFCODEPHI_AOSP_MANIFEST_REF}" \
		-u https://android.googlesource.com/platform/manifest \
		<<< 'n'

	local _num_jobs=4
	"${TERMUX_PKG_CACHEDIR}/repo" sync -c -j${_num_jobs} ||
		"${TERMUX_PKG_CACHEDIR}/repo" sync -c -j${_num_jobs} ||
		"${TERMUX_PKG_CACHEDIR}/repo" sync -c -j${_num_jobs} ||
		termux_error_exit "Repo sync failed"
}

termux_step_make() {
	local _arch
	case "${TERMUX_ARCH}" in
		i686) _arch=x86 ;;
		aarch64) _arch=arm64 ;;
		*) _arch=${TERMUX_ARCH} ;;
	esac

	local _go_cache_dir="${TERMUX_PKG_TMPDIR}/gocache"

	env -i PATH="${PATH}" GOCACHE="${_go_cache_dir}" bash -c "
		set -e
		cd ${TERMUX_PKG_SRCDIR}
		source build/envsetup.sh
		lunch aosp_${_arch}-aosp_current-eng
		export ALLOW_MISSING_DEPENDENCIES=true
		make linker libc libm libdl libdl_android debuggerd crash_dump
		make toybox sh mkshrc ping ping6 tracepath tracepath6 traceroute6 arping
	"
}

termux_step_make_install() {
	mkdir -p "${TERMUX_PREFIX}/opt/aosp/"
	cp -r "${TERMUX_PKG_SRCDIR}"/out/target/product/generic*/system "${TERMUX_PREFIX}/opt/aosp/system"
	cp -r "${TERMUX_PKG_SRCDIR}"/out/target/product/generic*/apex "${TERMUX_PREFIX}/opt/aosp/apex"
}
