TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/Vectras-VM-Android
TERMUX_PKG_DESCRIPTION="RAFCODEPHI Vectras integration dependency profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.1.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true

# Vectras source/runtime closure observed in its own doctor script. The build
# toolchain is inherited through rafcodephi-build-profile; non-build runtime
# helpers are listed here explicitly instead of being hidden in shell scripts.
TERMUX_PKG_DEPENDS="rafcodephi-build-profile, aria2, tar, pulseaudio, proot, proot-distro"

# Vectras itself is NOT copied into this package. Original Vectras authorship
# remains credited to xoureldeen and contributors under its GPLv2 fork notice.
# xterm requires the X11 repository to be enabled/refreshed before dependency
# resolution, so it is recorded as a staged dependency rather than a false
# same-transaction dependency. qemu-system-x86_64 on physical ARM32 remains
# TOKEN_VAZIO until an exact compatible QEMU artifact/build/runtime receipt.
termux_step_make_install() {
	local out="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$out"
	cat > "$out/vectras.profile" <<'EOF'
schema=rafcodephi.package-profile/v1
profile=vectras
build_parent=rafcodephi-build-profile
runtime_packages=aria2 tar pulseaudio proot proot-distro
x11_repository_package=x11-repo
x11_staged_package=xterm
x11_install_order=x11-repo -> apt/pkg metadata refresh -> xterm
qemu_common_package=qemu-common
qemu_utils_package=qemu-utils
qemu_system_x86_64_package=qemu-system-x86-64-headless
qemu_arm32_x86_64=TOKEN_VAZIO
EOF
}
