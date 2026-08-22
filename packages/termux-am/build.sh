# Contributor: @michalbednarski
TERMUX_PKG_HOMEPAGE=https://github.com/termux/TermuxAm
TERMUX_PKG_DESCRIPTION="Android Oreo-compatible am command reimplementation"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="Michal Bednarski @michalbednarski"
TERMUX_PKG_VERSION=0.8.0
TERMUX_PKG_REVISION=3
TERMUX_PKG_SRCURL=https://github.com/termux/TermuxAm/archive/refs/tags/v$TERMUX_PKG_VERSION.tar.gz
TERMUX_PKG_SHA256=7d4cfa2bfff93d5fc89fc89e537d2c072e08918276b140b7ed48ea45ebfbe8f3
TERMUX_PKG_PLATFORM_INDEPENDENT=true
TERMUX_PKG_BUILD_IN_SRC=true
TERMUX_PKG_CONFLICTS="termux-tools (<< 0.51)"
_GRADLE_VERSION=8.10.2

termux_step_post_get_source() {
	sed -i'' -E -e "s|\@TERMUX_PREFIX\@|${TERMUX_PREFIX}|g" "$TERMUX_PKG_SRCDIR/am-libexec-packaged"
	sed -i'' -E -e "s|\@TERMUX_APP_PACKAGE\@|${TERMUX_APP_PACKAGE}|g" "$TERMUX_PKG_SRCDIR/app/src/main/java/com/termux/termuxam/FakeContext.java"
}

termux_step_make() {
	# Download and use a new enough gradle version to avoid the process hanging after running:
	termux_download \
		https://services.gradle.org/distributions/gradle-$_GRADLE_VERSION-bin.zip \
		$TERMUX_PKG_CACHEDIR/gradle-$_GRADLE_VERSION-bin.zip \
		31c55713e40233a8303827ceb42ca48a47267a0ad4bab9177123121e71524c26
	mkdir $TERMUX_PKG_TMPDIR/gradle
	unzip -q $TERMUX_PKG_CACHEDIR/gradle-$_GRADLE_VERSION-bin.zip -d $TERMUX_PKG_TMPDIR/gradle

	export ANDROID_HOME
	export GRADLE_OPTS="-Dorg.gradle.daemon=false -Xmx1536m -Dorg.gradle.java.home=$TERMUX_JAVA_HOME"

	# The package-builder AppArmor profile intentionally keeps /home/builder/lib
	# immutable. TermuxAm v0.8.0 needs compileSdk 33 while the current builder
	# image does not preinstall platforms;android-33/build-tools;34.0.0, so Gradle
	# cannot auto-provision them in the canonical SDK root. For RAFCODEPHI only,
	# assemble a writable SDK view under TERMUX_PKG_TMPDIR: reuse the immutable
	# toolchain via symlinks and install only the two missing SDK components there.
	if [[ "$TERMUX_APP_PACKAGE" == "com.termux.rafacodephi" ]]; then
		local source_sdk="$ANDROID_HOME"
		local writable_sdk="$TERMUX_PKG_TMPDIR/android-sdk"
		local entry name sdkmanager
		mkdir -p "$writable_sdk/platforms" "$writable_sdk/build-tools"

		for entry in "$source_sdk"/*; do
			[[ -e "$entry" ]] || continue
			name="$(basename "$entry")"
			case "$name" in
				platforms|build-tools) continue ;;
			esac
			ln -s "$entry" "$writable_sdk/$name"
		done
		for entry in "$source_sdk/platforms"/*; do
			[[ -e "$entry" ]] || continue
			ln -s "$entry" "$writable_sdk/platforms/$(basename "$entry")"
		done
		for entry in "$source_sdk/build-tools"/*; do
			[[ -e "$entry" ]] || continue
			ln -s "$entry" "$writable_sdk/build-tools/$(basename "$entry")"
		done

		if [[ -x "$writable_sdk/cmdline-tools/latest/bin/sdkmanager" ]]; then
			sdkmanager="$writable_sdk/cmdline-tools/latest/bin/sdkmanager"
		elif [[ -x "$writable_sdk/cmdline-tools/bin/sdkmanager" ]]; then
			sdkmanager="$writable_sdk/cmdline-tools/bin/sdkmanager"
		else
			termux_error_exit "No sdkmanager available in RAFCODEPHI writable SDK view"
		fi

		yes | "$sdkmanager" --sdk_root="$writable_sdk" \
			"platforms;android-33" \
			"build-tools;34.0.0"
		export ANDROID_HOME="$writable_sdk"
		export ANDROID_SDK_ROOT="$writable_sdk"
	fi

	# Why 'echo -n |'? See https://github.com/gradle/gradle/issues/14961 -
	# the build can hang otherwise.
	echo -n | $TERMUX_PKG_TMPDIR/gradle/gradle-$_GRADLE_VERSION/bin/gradle \
		--no-daemon \
		:app:assembleRelease
}

termux_step_make_install() {
	cp $TERMUX_PKG_SRCDIR/am-libexec-packaged $TERMUX_PREFIX/bin/am
	mkdir -p $TERMUX_PREFIX/libexec/termux-am
	cp $TERMUX_PKG_SRCDIR/app/build/outputs/apk/release/app-release-unsigned.apk $TERMUX_PREFIX/libexec/termux-am/am.apk
}
