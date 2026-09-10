TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/Vectras-VM-Android
TERMUX_PKG_DESCRIPTION="RAFCODEPHI freestanding RMR deterministic vector-field static library"
TERMUX_PKG_LICENSE="GPL-2.0-only"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=0.1.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true

# Origin-Repository: rafaelmeloreisnovo/Vectras-VM-Android
# Origin-Commit: 34aa3db434627c40cdc6fdb595591634e74d090d
# Origin-Path: engine/rmr/src/rmr_vector_field.c + engine/rmr/include/{rmr_vector_field.h,rmr_types.h}
# Original-Author-or-Project: Rafael M. R. source-file copyright; parent Vectras VM Android by xoureldeen
# Governing-License: GPL-2.0-only
# Local-Modification: source files are byte-identical copies; this recipe only packages/compiles/installs them.

termux_step_make() {
	local src="$TERMUX_PKG_BUILDER_DIR/files"
	mkdir -p "$TERMUX_PKG_BUILDDIR"

	"$CC" $CPPFLAGS $CFLAGS \
		-std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector \
		-I"$src" \
		-c "$src/rmr_vector_field.c" \
		-o "$TERMUX_PKG_BUILDDIR/rmr_vector_field.o"

	"$AR" rcs \
		"$TERMUX_PKG_BUILDDIR/librafcodephi-rmr-vector-field.a" \
		"$TERMUX_PKG_BUILDDIR/rmr_vector_field.o"

	# Freestanding means no hidden compiler-runtime/libc dependency is accepted.
	# Fail closed if Clang lowers an operation to __aeabi_*, memcpy, libm, etc.
	local unresolved
	unresolved="$("$NM" -u "$TERMUX_PKG_BUILDDIR/rmr_vector_field.o" || true)"
	if [[ -n "$unresolved" ]]; then
		printf 'unexpected unresolved symbols in RMR vector field:\n%s\n' "$unresolved" >&2
		exit 67
	fi
}

termux_step_make_install() {
	local src="$TERMUX_PKG_BUILDER_DIR/files"
	local inc="$TERMUX_PREFIX/include/rafcodephi/rmr"
	local doc="$TERMUX_PREFIX/share/doc/rafcodephi-rmr-vector-field"

	install -Dm644 \
		"$TERMUX_PKG_BUILDDIR/librafcodephi-rmr-vector-field.a" \
		"$TERMUX_PREFIX/lib/librafcodephi-rmr-vector-field.a"
	install -Dm644 "$src/rmr_vector_field.h" "$inc/rmr_vector_field.h"
	install -Dm644 "$src/rmr_types.h" "$inc/rmr_types.h"
	install -Dm644 "$TERMUX_PKG_BUILDER_DIR/ORIGIN.md" "$doc/ORIGIN.md"
}
