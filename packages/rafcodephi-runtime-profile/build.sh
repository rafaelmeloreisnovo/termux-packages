TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/termux-packages
TERMUX_PKG_DESCRIPTION="RAFCODEPHI runtime dependency profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true
TERMUX_PKG_DEPENDS="rafcodephi-bootstrap-profile, openssl, curl, git, python, procps, b3sum"

# BLAKE3 capability is provided by the existing official-upstream b3sum recipe;
# this metapackage does not copy or relicense BLAKE3 source.
termux_step_make_install() {
	local out="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$out"
	printf '%s\n' 'profile=runtime' 'schema=rafcodephi.package-profile/v1' > "$out/runtime.profile"
}
