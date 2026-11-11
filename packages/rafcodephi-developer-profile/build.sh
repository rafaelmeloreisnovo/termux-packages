TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/termux-packages
TERMUX_PKG_DESCRIPTION="RAFCODEPHI Android shell, JavaScript, JDK and NDK development profile"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true
TERMUX_PKG_DEPENDS="nodejs-lts, npm, openjdk-21, android-tools, clang, make, ninja, ndk-sysroot, proot, proot-distro"

termux_step_make_install() {
	local profile_dir="$TERMUX_PREFIX/share/rafcodephi/profiles"
	mkdir -p "$profile_dir" "$TERMUX_PREFIX/etc/profile.d" "$TERMUX_PREFIX/bin"
	cat > "$profile_dir/developer.profile" <<'EOF'
schema=rafcodephi.package-profile/v1
profile=developer
javascript=nodejs-lts+npm
jdk=openjdk-21
android_tools=adb+fastboot
native_toolchain=clang+make+ninja+ndk-sysroot
container_runtime=proot+proot-distro
ndk_scope=TERMUX_ON_DEVICE_SYSROOT_NOT_GOOGLE_HOST_NDK
EOF

	cat > "$TERMUX_PREFIX/etc/profile.d/rafcodephi-developer.sh" <<EOF
# RAFCODEPHI developer profile. Sourced by the Termux bash system profile.
export JAVA_HOME="$TERMUX_PREFIX/lib/jvm/java-21-openjdk"
export RAFCODEPHI_NDK_SYSROOT="$TERMUX_PREFIX"
export NDK_SYSROOT="$TERMUX_PREFIX"
case ":\$PATH:" in
	*":\$JAVA_HOME/bin:"*) ;;
	*) export PATH="\$JAVA_HOME/bin:\$PATH" ;;
esac
EOF

	cat > "$TERMUX_PREFIX/bin/rafcodephi-dev-doctor" <<EOF
#!$TERMUX_PREFIX/bin/bash
set -u
prefix="\${PREFIX:-$TERMUX_PREFIX}"
tmp="\${TMPDIR:-\$prefix/tmp}/rafcodephi-dev-doctor.\$\$"
failed=0
cleanup() { rm -rf "\$tmp"; }
trap cleanup EXIT INT TERM
mkdir -p "\$tmp"

check_command() {
	local command_name="\$1"
	if ! command -v "\$command_name" >/dev/null 2>&1; then
		printf 'RAFCODEPHI_DEV_TOOL=BLOCKED tool=%s reason=COMMAND_MISSING\n' "\$command_name" >&2
		failed=1
	fi
}

for command_name in bash node npm java javac adb fastboot clang make ninja proot proot-distro; do
	check_command "\$command_name"
done
[[ -f "\$prefix/include/android/api-level.h" ]] || {
	printf 'RAFCODEPHI_DEV_TOOL=BLOCKED tool=ndk-sysroot reason=HEADER_MISSING\n' >&2
	failed=1
}
(( failed == 0 )) || exit 1

[[ "\${JAVA_HOME:-}" == "\$prefix/lib/jvm/java-21-openjdk" ]] || {
	printf 'RAFCODEPHI_DEV_TOOL=BLOCKED tool=bash-profile reason=JAVA_HOME_NOT_INITIALIZED\n' >&2
	exit 1
}
[[ "\${RAFCODEPHI_NDK_SYSROOT:-}" == "\$prefix" && "\${NDK_SYSROOT:-}" == "\$prefix" ]] || {
	printf 'RAFCODEPHI_DEV_TOOL=BLOCKED tool=bash-profile reason=NDK_SYSROOT_NOT_INITIALIZED\n' >&2
	exit 1
}

node -e 'if (2 + 2 !== 4) process.exit(1); console.log("RAFCODEPHI_NODE=PASS")'
cat > "\$tmp/RafcodephiSmoke.java" <<'JAVA'
public final class RafcodephiSmoke {
    public static void main(String[] args) { System.out.println("RAFCODEPHI_JAVA=PASS"); }
}
JAVA
javac -d "\$tmp" "\$tmp/RafcodephiSmoke.java"
java -cp "\$tmp" RafcodephiSmoke

cat > "\$tmp/native.c" <<'C'
int rafcodephi_native_smoke(void) { return 42; }
C
clang --sysroot="\$prefix" -c "\$tmp/native.c" -o "\$tmp/native.o"

cat > "\$tmp/build.ninja" <<'NINJA'
rule stamp
  command = printf 'RAFCODEPHI_NINJA=PASS\n' > \$out
build ninja.pass: stamp
NINJA
ninja -f "\$tmp/build.ninja" -C "\$tmp" ninja.pass >/dev/null
grep -Fx 'RAFCODEPHI_NINJA=PASS' "\$tmp/ninja.pass"

adb version | sed -n '1p'
fastboot --version | sed -n '1p'
proot --version | sed -n '1p'
proot-distro list >/dev/null
printf 'RAFCODEPHI_DEVELOPER_PROFILE=PASS scope=ON_DEVICE_COMMAND_STARTUP\n'
EOF
	chmod 0755 "$TERMUX_PREFIX/bin/rafcodephi-dev-doctor"
}
