# X11 package
TERMUX_PKG_HOMEPAGE=https://xorg.freedesktop.org/
TERMUX_PKG_DESCRIPTION="X.Org Autotools macros"
TERMUX_PKG_LICENSE="HPND, MIT"
TERMUX_PKG_MAINTAINER="@termux"
TERMUX_PKG_VERSION="1.20.2"
TERMUX_PKG_AUTO_UPDATE=true
TERMUX_PKG_SRCURL=https://xorg.freedesktop.org/releases/individual/util/util-macros-${TERMUX_PKG_VERSION}.tar.xz
# RAFCODEPHI uses the same canonical release bytes through an alternate mirror.
# TERMUX_PKG_SHA256 remains only as source-transport integrity; it is not a
# runtime/result/evidence gate for the RAFCODEPHI freestanding path.
if [[ "${TERMUX_APP_PACKAGE:-}" == "com.termux.rafacodephi" ]]; then
	TERMUX_PKG_SRCURL=https://repo-default.voidlinux.org/distfiles/util-macros-${TERMUX_PKG_VERSION}.tar.xz
fi
TERMUX_PKG_SHA256=9ac269eba24f672d7d7b3574e4be5f333d13f04a7712303b1821b2a51ac82e8e
TERMUX_PKG_PLATFORM_INDEPENDENT=true
