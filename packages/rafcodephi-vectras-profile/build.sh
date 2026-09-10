TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/Vectras-VM-Android
TERMUX_PKG_DESCRIPTION="RAFCODEPHI Vectras integration dependency profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true
TERMUX_PKG_DEPENDS="rafcodephi-runtime-profile, proot"

# Vectras itself is NOT copied into this package. Original Vectras authorship
# remains credited to xoureldeen and contributors under its GPLv2 fork notice.
# qemu-system-x86_64 on a physical ARM32 host remains TOKEN_VAZIO until an exact
# compatible QEMU artifact/build/runtime receipt is available; do not hide that
# gap by adding an incompatible package dependency here.
termux_step_make_install() {
	local out="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$out"
	printf '%s\n' \
		'profile=vectras' \
		'schema=rafcodephi.package-profile/v1' \
		'qemu_arm32_x86_64=TOKEN_VAZIO' \
		> "$out/vectras.profile"
}
