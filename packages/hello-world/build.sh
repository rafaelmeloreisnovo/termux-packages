#!/bin/bash
# Termux package build script for hello-world

TERMUX_PKG_HOMEPAGE=https://www.gnu.org/software/hello/
TERMUX_PKG_DESCRIPTION="A simple greeting program"
TERMUX_PKG_VERSION=2.12
TERMUX_PKG_SRCURL=https://mirror.example.com/hello-${TERMUX_PKG_VERSION}.tar.gz
TERMUX_PKG_SHA256=e1fd0b40d68faea6c19a6e04b32a32eca9fc93c7e2c85be12daf94f4c82bf6da
TERMUX_PKG_DEPENDS="libc"

termux_step_pre_configure() {
  return 0
}

termux_step_configure() {
  ./configure --prefix="$TERMUX_PREFIX" --host="$TERMUX_HOST_PLATFORM"
}

termux_step_make() {
  make -j "$TERMUX_MAKE_PROCESSES"
}

termux_step_install() {
  make install
}

termux_step_post_install() {
  return 0
}
