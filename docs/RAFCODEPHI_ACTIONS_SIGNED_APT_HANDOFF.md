# RAFAELIA Actions → termux-packages → signed APT/Termux App

## Current source authority and integration

Source lanes: `rafaelmeloreisnovo/actions@7c03495ebe1d28a11ea419799c4f61427250244f` (owned source integrity gate), `rafaelmeloreisnovo/termux-packages` (NDK r29, source-built .deb, signed indexes), `rafaelmeloreisnovo/termux-app-rafacodephi` (bootstrap importer and APK). This *does not* transfer license or origin: retain upstream notices, fork credits, original Gradle/Android/Linux/NDK provenance.

The existing `.github/workflows/rafcodephi-publish-dev-apt.yml` independently runs the owned immutable-source check on both fast PR source-contract and explicit `workflow_dispatch` dual-arch signed producer. Checkout uses exact PR head SHA, not the synthetic merge ref. The bounded dual-architecture proof can run on same-repository PRs **without production signing secrets or publication**; production publishing still requires explicit manual dispatch from main. The NDK29 adapter reuses the existing Termux `termux_download` function and the official SHA-256 retained in `scripts/setup-android-sdk.sh` to provision only the NDK archive under the allowed `/home/builder/.termux-build/android-ndk-r29`. It leaves AppArmor's explicit prohibitions on writing trusted source and `/home/builder/lib` intact. It checks `source.properties` and Clang before build; no SDK Manager license mutation or root-in-container workaround. All of this is in the Docker builder, not the phone.

## Runtime graph and trust split

```
source SHA → owned verify-source gate → NDK r29 source SDK checksum gate
 → ARMv7/AArch64 source builds → .deb per-file digests
 → release metadata (Packages, Packages.gz, Release)
 → production GPG InRelease + Release.gpg (only with secrets)
 → isolated host APT resolution and matching archive key
 → bootstrap profile, key and ZIP/manifest hashes
 → Termux app importer with separately pinned key SHA
 → APK structural build + physical apt update/install only after device tests
```

**Current blocker:** no published `rafcodephi-apt` / `rafcodephi-apt-preview` branch observed at audit. The ordinary real-bootstrap inserts `Enabled: no` and `APT::Update::Pre-Invoke` blocking `apt update`. Do not remove it without the signed handoff. Manual publication already requires production signing key/fingerprint and a publish token, validated outside repo; no secrets are created by this PR, no publish or merge executed.

Repository authenticity ≠ object source hash. A key must be pinned independently at app build; a public key simply copied from an untrusted ZIP cannot authorize its own trust. APT package-index integrity is a provider trust decision and physical install is separate evidence.

## Acceptance and rollback

Scoped source acceptance: fast workflow static tests, NDK script syntax, signed-fixture host tests, exact checkout verification. Full acceptance still requires producer NDK installation and source build, signed Release, real repository publication, archive key fingerprint/custody, same-HEAD consumer validation, APK hashes, ARM32/ARM64 physical `pkg update; pkg install` plus dependency resolution on actual phone. Missing gate => HOLD (not automatic failover to public Termux incompatible prefix).

Rollback: close DRAFT PR, revert only additive workflow and NDK script; preserve existing unsigned contract and all predecessor commits. `SOURCE≠ARTIFACT≠EXECUTION≠EVIDENCE≠CLAIM`; `claim_allowed=false`; certification=NOT_CLAIMED.
