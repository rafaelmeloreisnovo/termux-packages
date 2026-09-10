TERMUX_PKG_HOMEPAGE=https://github.com/rafaelmeloreisnovo/termux-packages
TERMUX_PKG_DESCRIPTION="RAFCODEPHI federated component provenance and third-party attribution metadata"
TERMUX_PKG_LICENSE="Apache-2.0"
TERMUX_PKG_MAINTAINER="@rafaelmeloreisnovo"
TERMUX_PKG_VERSION=1.0.0
TERMUX_PKG_SKIP_SRC_EXTRACT=true
TERMUX_PKG_PLATFORM_INDEPENDENT=true

# This package contains RAFCODEPHI-authored metadata only. It deliberately does
# not vendor BLAKE3, Vectras, QEMU, AndroidX, Termux or Termux:API source code.
# Third-party authorship and licenses are indexed by the installed manifest and
# NOTICE; those records never relicense the upstream works.
termux_step_make_install() {
	local share="$TERMUX_PREFIX/share/rafcodephi/federation"
	mkdir -p "$share"
	install -Dm644 \
		"$TERMUX_PKG_BUILDER_DIR/../../docs/assurance/rafcodephi-federated-components.v1.json" \
		"$share/rafcodephi-federated-components.v1.json"
	install -Dm644 \
		"$TERMUX_PKG_BUILDER_DIR/../../docs/assurance/THIRD_PARTY_AND_FORK_ATTRIBUTION.md" \
		"$share/THIRD_PARTY_AND_FORK_ATTRIBUTION.md"
	cat > "$share/README" <<'EOF'
RAFCODEPHI federation metadata.
This directory records source/fork provenance and attribution only.
It does not replace any upstream license text or NOTICE requirement.
SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM.
EOF
}
