TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/termux-app-rafacodephi
TERMUX_PKG_DESCRIPTION="RAFCODEPHI full dependency profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true
TERMUX_PKG_DEPENDS="rafcodephi-vectras-profile, termux-api"

# termux-api here is the Termux CLI client package. The Android companion app
# rafaelmeloreisnovo/termux-api_rafcodephi remains a separate GPLv3 project and
# is integrated by package/provider contract rather than source copying.
termux_step_make_install() {
	local out="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$out"
	printf '%s\n' 'profile=full' 'schema=rafcodephi.package-profile/v1' > "$out/full.profile"
}
