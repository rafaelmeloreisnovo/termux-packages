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
TERMUX_PKG_VERSION="16.0.0_r4"
TERMUX_PKG_AUTO_UPDATE=false
TERMUX_PKG_BUILD_IN_SRC=true
TERMUX_PKG_SKIP_SRC_EXTRACT=true
# Should be handled by AOSP build system, so it should be disabled here.
TERMUX_PKG_UNDEF_SYMBOLS_FILES="all"
TERMUX_PKG_DEPENDS="resolv-conf"
TERMUX_PKG_BREAKS="bionic-host"
TERMUX_PKG_REPLACES="bionic-host"
# For safety to protect termux-docker users out of an abundance of caution,
# because a failed or bugged build of this package may corrupt termux-docker more severely
# than it may corrupt a standard Android ROM.
TERMUX_PKG_ON_DEVICE_BUILD_NOT_SUPPORTED=true

termux_step_get_source() {
	local TMP_CHECKOUT="$TERMUX_PKG_CACHEDIR/tmp-checkout"
	local TMP_CHECKOUT_VERSION="$TERMUX_PKG_CACHEDIR/tmp-checkout-version"

	if [[ ! -f "$TMP_CHECKOUT_VERSION" || "$(cat $TMP_CHECKOUT_VERSION)" != "$TERMUX_PKG_VERSION" ]]; then
		echo "Downloading AOSP source from '$TERMUX_PKG_SRCURL'"

		rm -rf "$TMP_CHECKOUT"

		export LD_LIBRARY_PATH="${TMP_CHECKOUT}/prefix/lib/x86_64-linux-gnu:${TMP_CHECKOUT}/prefix/usr/lib/x86_64-linux-gnu"
		export PATH="${TMP_CHECKOUT}/prefix/usr/bin:${PATH//$HOME\/.cargo\/bin/}"

		local -a ubuntu_packages=(
			"libncurses5"
			"libtinfo5"
			"openssh-client"
		)

		DESTINATION="${TMP_CHECKOUT}/prefix" \
		UBUNTU_RELEASE=jammy \
		termux_download_ubuntu_packages "${ubuntu_packages[@]}"

		termux_download https://storage.googleapis.com/git-repo-downloads/repo "${TERMUX_PKG_CACHEDIR}/repo" SKIP_CHECKSUM
		chmod +x "${TERMUX_PKG_CACHEDIR}/repo"

		pushd "$TMP_CHECKOUT"

		# Repo requires us to have a Git user name and email set.
		# The GitHub workflow does this, but the local build container doesn't
		[[ "$(git config --get user.name)" != '' ]] || git config --global user.name "Termux Github Actions"
		[[ "$(git config --get user.email)" != '' ]] || git config --global user.email "contact@termux.dev"
		"${TERMUX_PKG_CACHEDIR}"/repo init \
			-u "${TERMUX_PKG_SRCURL}" \
			-b main -m "${TERMUX_PKG_BUILDER_DIR}/default.xml" <<< 'n'
		"${TERMUX_PKG_CACHEDIR}"/repo sync -c -j32

		popd

		echo "$TERMUX_PKG_VERSION" > "$TMP_CHECKOUT_VERSION"
	else
		echo "Skipped downloading of AOSP source from '$TERMUX_PKG_SRCURL'"
	fi

	termux_download https://storage.googleapis.com/git-repo-downloads/repo "${TERMUX_PKG_CACHEDIR}/repo" SKIP_CHECKSUM
	chmod +x "${TERMUX_PKG_CACHEDIR}/repo"

	mkdir -p "${TERMUX_PKG_SRCDIR}"
	cd "${TERMUX_PKG_SRCDIR}" || termux_error_exit "Couldn't enter source code directory: ${TERMUX_PKG_SRCDIR}"

	# Repo requires us to have a Git user name and email set.
	# The GitHub workflow does this, but the local build container doesn't
	[[ "$(git config --get user.name)" != '' ]] || git config --global user.name "Termux Github Actions"
	[[ "$(git config --get user.email)" != '' ]] || git config --global user.email "contact@termux.dev"

	"${TERMUX_PKG_CACHEDIR}"/repo init \
		--partial-clone \
		--no-use-superproject \
		-b android-${TERMUX_PKG_VERSION} \
		-u https://android.googlesource.com/platform/manifest \
		<<< 'n'

	local _NUM_JOBS=4
	"${TERMUX_PKG_CACHEDIR}"/repo sync -c -j${_NUM_JOBS} ||
		"${TERMUX_PKG_CACHEDIR}"/repo sync -c -j${_NUM_JOBS} ||
		"${TERMUX_PKG_CACHEDIR}"/repo sync -c -j${_NUM_JOBS} ||
		termux_error_exit "Repo sync failed"
}

termux_step_make() {
	case "${TERMUX_ARCH}" in
		i686) _ARCH=x86 ;;
		aarch64) _ARCH=arm64 ;;
		*) _ARCH=${TERMUX_ARCH} ;;
	esac

	local _GO_CACHE_DIR="${TERMUX_PKG_TMPDIR}/gocache"

	env -i PATH="${PATH}" GOCACHE="${_GO_CACHE_DIR}" bash -c "
		set -e;
		cd ${TERMUX_PKG_SRCDIR}
		source build/envsetup.sh;
<<<<<<< HEAD
		lunch aosp_${_ARCH}-aosp_current-eng;
		export ALLOW_MISSING_DEPENDENCIES=true
		make linker libc libm libdl libdl_android debuggerd crash_dump
		make toybox sh mkshrc ping ping6 tracepath tracepath6 traceroute6 arping
=======
		lunch aosp_${_AOSP_ARCH}-eng;
		export ALLOW_MISSING_DEPENDENCIES=true
		make linker libc libm libdl libicuuc debuggerd crash_dump
		make toybox grep sh mkshrc ping ping6 tracepath tracepath6 traceroute6 arping
>>>>>>> upstream/master
	"
}

termux_step_make_install() {
	mkdir -p "${TERMUX_PREFIX}/opt/aosp/"
	cp -r "${TERMUX_PKG_SRCDIR}"/out/target/product/generic*/system "${TERMUX_PREFIX}/opt/aosp/system"
	cp -r "${TERMUX_PKG_SRCDIR}"/out/target/product/generic*/apex "${TERMUX_PREFIX}/opt/aosp/apex"
}
