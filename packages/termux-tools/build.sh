TERMUX_PKG_HOMEPAGE=https://github.com/termux-play-store/termux-tools
TERMUX_PKG_DESCRIPTION="Basic system tools for Termux"
TERMUX_PKG_LICENSE="GPL-3.0"
TERMUX_PKG_MAINTAINER="@termux"
TERMUX_PKG_VERSION="3.0.9"
TERMUX_PKG_SRCURL=https://github.com/termux-play-store/termux-tools/archive/refs/tags/${TERMUX_PKG_VERSION}.tar.gz
TERMUX_PKG_SHA256=d275fbc736c936abc9a8460d4d310d34704406db03405cf96f82a62b082893a9
TERMUX_PKG_ESSENTIAL=true
TERMUX_PKG_AUTO_UPDATE=true
TERMUX_PKG_UPDATE_TAG_TYPE="newest-tag"
TERMUX_PKG_SUGGESTS="termux-api"

# Some of these packages are not dependencies and used only to ensure
# that core packages are installed after upgrading (we removed busybox
# from essentials).
TERMUX_PKG_DEPENDS="coreutils, curl, dash, diffutils, findutils, gawk, grep, less, procps, psmisc, sed, tar, termux-am, termux-exec, util-linux"

# Optional packages that are distributed as part of bootstrap archives.
TERMUX_PKG_RECOMMENDS="ed, dos2unix, inetutils, net-tools, patch, unzip"

termux_step_pre_configure() {
	# A forked Termux application must not inherit com.termux paths/package IDs
	# through termux-tools (pkg, termux-setup-package-manager, etc.). Configure
	# scripts also consume TERMUX_PREFIX from the environment, so export both
	# compatibility variables explicitly before autoreconf/configure.
	export TERMUX_PREFIX TERMUX_APP_PACKAGE

	# Rewrite text source only; never post-process packaged ELF/data binaries.
	# Replacing the package id also transforms the canonical /data/data/com.termux
	# prefix embedded in scripts into the package-specific RAFCODEPHI path.
	while IFS= read -r -d '' source_file; do
		sed -i "s/com\.termux/${TERMUX_APP_PACKAGE//\//\\/}/g" "$source_file"
	done < <(grep -IlZR -- 'com\.termux' . || true)

	# Can't apply these patch normally since they contains special @TERMUX..@ text which normal patch replaces:
	for d in "$TERMUX_PKG_BUILDER_DIR"/*.diff; do
		patch -p1 < "$d"
	done

	autoreconf -vfi
}
