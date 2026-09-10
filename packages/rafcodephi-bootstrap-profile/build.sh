TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/termux-packages
TERMUX_PKG_DESCRIPTION="RAFCODEPHI bootstrap dependency profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true
TERMUX_PKG_DEPENDS="apt, bash, busybox, dpkg, ca-certificates, coreutils, termux-tools, rafcodephi-federation-metadata"

# Local metapackage only: the dependencies keep their own authors and licenses.
termux_step_make_install() {
	local out="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$out"
	printf '%s\n' 'profile=bootstrap' 'schema=rafcodephi.package-profile/v1' > "$out/bootstrap.profile"
}
