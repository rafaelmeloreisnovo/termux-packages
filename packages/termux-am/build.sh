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

	# AppArmor keeps /home/builder/lib immutable. TermuxAm v0.8.0 needs
	# platforms;android-33 and build-tools;34.0.0 that the image lacks.
	# Keep the original RAFCODEPHI behavior, and opt in canonical com.termux
	# only for the scoped QEMU CI target proof. Neither lane relaxes AppArmor.
	# Install the missing components in a writable, per-package SDK view.
	if [[ "$TERMUX_APP_PACKAGE" == "com.termux.rafacodephi" ||
		( "$TERMUX_APP_PACKAGE" == "com.termux" && "${RAFCODEPHI_CI_CANONICAL_SDK_VIEW:-false}" == "true" ) ]]; then
		local source_sdk="$ANDROID_HOME"
		local writable_sdk="$TERMUX_PKG_TMPDIR/android-sdk"
		local entry name sdkmanager sdkmanager_status
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

		# With pipefail enabled, sdkmanager can finish successfully after consuming
		# its required confirmations while `yes` receives SIGPIPE (141). Preserve
		# fail-closed semantics on sdkmanager itself without treating that expected
		# producer-side SIGPIPE as an SDK installation failure.
		if yes | "$sdkmanager" --sdk_root="$writable_sdk" \
			"platforms;android-33" \
			"build-tools;34.0.0"; then
			sdkmanager_status=0
		else
			sdkmanager_status="${PIPESTATUS[1]}"
		fi
		if (( sdkmanager_status != 0 )); then
			termux_error_exit "RAFCODEPHI sdkmanager provisioning failed with exit $sdkmanager_status"
		fi

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
