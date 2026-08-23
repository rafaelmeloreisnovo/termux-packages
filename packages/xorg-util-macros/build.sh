# X11 package
TERMUX_PKG_HOMEPAGE=https://xorg.freedesktop.org/
TERMUX_PKG_DESCRIPTION="X.Org Autotools macros"
TERMUX_PKG_LICENSE="HPND, MIT"
TERMUX_PKG_MAINTAINER="@termux"
TERMUX_PKG_VERSION="1.20.2"
TERMUX_PKG_AUTO_UPDATE=true
TERMUX_PKG_SRCURL=https://xorg.freedesktop.org/releases/individual/util/util-macros-${TERMUX_PKG_VERSION}.tar.xz
# RAFCODEPHI source-build evidence timed out independently against both the
# freedesktop release endpoint and its x.org publication alias. Keep normal
# Termux builds unchanged, but use the immutable upstream GitLab tag archive
# for the custom-prefix route and generate its autotools files locally.
if [[ "${TERMUX_APP_PACKAGE:-}" == "com.termux.rafacodephi" ]]; then
	TERMUX_PKG_SRCURL=https://gitlab.freedesktop.org/xorg/util/macros/-/archive/util-macros-${TERMUX_PKG_VERSION}/macros-util-macros-${TERMUX_PKG_VERSION}.tar.gz
	TERMUX_PKG_SHA256=beac7e00e5996bd0c9d9bd8cf62704583b22dbe8613bd768626b95fcac955744
else
	TERMUX_PKG_SHA256=9ac269eba24f672d7d7b3574e4be5f333d13f04a7712303b1821b2a51ac82e8e
fi
TERMUX_PKG_PLATFORM_INDEPENDENT=true

termux_step_pre_configure() {
	if [[ "${TERMUX_APP_PACKAGE:-}" == "com.termux.rafacodephi" ]]; then
		autoreconf -fi
	fi
}
