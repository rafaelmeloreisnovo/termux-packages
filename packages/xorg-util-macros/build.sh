# X11 package
TERMUX_PKG_HOMEPAGE=https://xorg.freedesktop.org/
TERMUX_PKG_DESCRIPTION="X.Org Autotools macros"
TERMUX_PKG_LICENSE="HPND, MIT"
TERMUX_PKG_MAINTAINER="@termux"
TERMUX_PKG_VERSION="1.20.2"
TERMUX_PKG_AUTO_UPDATE=true
TERMUX_PKG_SRCURL=https://xorg.freedesktop.org/releases/individual/util/util-macros-${TERMUX_PKG_VERSION}.tar.xz
# Keep RAFCODEPHI on the canonical X.Org release URL as well. The previous
# RAFCODEPHI-only Void mirror override returned HTTP 404 for 1.20.2 and caused
# the bootstrap producer to fail before artifact/receipt generation.
TERMUX_PKG_SHA256=9ac269eba24f672d7d7b3574e4be5f333d13f04a7712303b1821b2a51ac82e8e
TERMUX_PKG_PLATFORM_INDEPENDENT=true
