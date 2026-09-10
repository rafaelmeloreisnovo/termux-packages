TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/termux-packages
TERMUX_PKG_DESCRIPTION="RAFCODEPHI source-build and native development dependency profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true

# What is needed when the RAFCODEPHI environment itself must compile/link code.
# Package-manager names are outputs; their producing recipes are mapped by CI.
# clang + lld are outputs of packages/libllvm, not independent source recipes.
TERMUX_PKG_DEPENDS="rafcodephi-runtime-profile, clang, lld, cmake, make, ninja, binutils, file, patchelf, rafcodephi-rmr-vector-field, rafcodephi-rmr-vector-field-static"

termux_step_make_install() {
	local out="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$out"
	cat > "$out/build.profile" <<'EOF'
schema=rafcodephi.package-profile/v1
profile=build
layer=source-build-native-development
compiler_package=clang
compiler_source_recipe=libllvm
linker_package=lld
linker_source_recipe=libllvm
build_system_packages=cmake make ninja
binary_inspection_packages=binutils file patchelf
freestanding_headers=rafcodephi-rmr-vector-field
freestanding_static_library=rafcodephi-rmr-vector-field-static
runtime_parent=rafcodephi-runtime-profile
EOF
}
