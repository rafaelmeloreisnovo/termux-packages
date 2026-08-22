# X11 package
TERMUX_PKG_HOMEPAGE=https://xorg.freedesktop.org/
TERMUX_PKG_DESCRIPTION="X.Org Autotools macros"
TERMUX_PKG_LICENSE="HPND, MIT"
TERMUX_PKG_MAINTAINER="@termux"
TERMUX_PKG_VERSION="1.20.2"
TERMUX_PKG_AUTO_UPDATE=true
TERMUX_PKG_SRCURL=https://xorg.freedesktop.org/releases/individual/util/util-macros-${TERMUX_PKG_VERSION}.tar.xz
# RAFCODEPHI source-build evidence repeatedly timed out against the freedesktop
# release endpoint in independent bootstrap/APT workflows. Keep normal Termux
# builds unchanged, but route this custom-prefix build through the official
# x.org publication endpoint for the identical checksum-pinned tarball.
if [[ "${TERMUX_APP_PACKAGE:-}" == "com.termux.rafacodephi" ]]; then
	TERMUX_PKG_SRCURL=https://www.x.org/pub/individual/util/util-macros-${TERMUX_PKG_VERSION}.tar.xz
fi
TERMUX_PKG_SHA256=9ac269eba24f672d7d7b3574e4be5f333d13f04a7712303b1821b2a51ac82e8e
TERMUX_PKG_PLATFORM_INDEPENDENT=true
