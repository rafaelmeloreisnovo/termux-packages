# Canonical builder NDK preflight — scoped hotfix 2026-10-24

## Observed predecessor and intent

At public GitHub run [40140678420](https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/40140678420), the real bootstrap producer entered the Termux container, then failed with:
`ERROR: NDK (/home/builder/lib/android-ndk-r29) not pointing at a directory!`.
A failure receipt/upload is not an APK, DEB or real bootstrap success. This is a missing builder prerequisite, not grounds to downgrade Gradle, substitute bridge executables or remove policy controls.

## Source → decision → evidence

- Producer: `scripts/ci/rafcodephi_ndk_preflight.sh` (Bash within canonical hosted Docker build environment, not ARM32 runtime).
- Orchestration: `.github/workflows/rafcodephi-real-bootstrap.yml` now checks out exact PR HEAD, uses the owned pinned Actions `rafcode/verify-source` gate before source mutation, tests syntax, verifies the canonical package property overlay and then probes **inside the same builder image** ahead of any costly source packages.
- Gate requires declared NDK major `29`, an existing directory, readable `source.properties`, major revision `29.x` and an executable LLVM Clang at the canonical Linux x86_64 prebuilt path. Missing/mismatch blocks without fetching unsigned tools. This is a local file/toolchain presence check, **not** an NDK provenance/signature attestation.
- Preflight output is captured to `build/reports/rafcodephi-ndk-preflight.log`. On preflight failure, the existing always-run triage now consumes this log if full build did not create a log, and uploads the failure receipt. No false success after upload.
- Source-build, DEB ZIP gate, required packages, app identity `com.termux.rafacodephi`, manifest, ARM32/ARM64, signature control and fail-closed repository policy remain unchanged.
- Earlier independent PR #158 classifies NDK causes in host failure triage. This PR changes the **actual builder gate**, not that open PR.

## Security/availability tradeoff

A cold runner still must pull the upstream builder image; no performance claim is made without timing. Fast-fail reduces unnecessary package compilation when NDK is missing, but cannot provision or repair the image. Passing this presence check neither proves cross-build correctness nor physical device execution. An immutable digest-pinned builder image or SHA-verified NDK provenance is a **separate review** and remains `TOKEN_VAZIO` here. Existing image is not replaced or trust-expanded silently.

## Acceptance, rights, rollback

Success: exact HEAD source gate PASS, syntax PASS, NDK preflight either produces scoped PASS with metadata or BLOCKED with explicit reason, and existing bootstrap gates stay fail-closed. Real build PASS only with all real source-built package/archive evidence and independently validated artifacts. For the known missing-NDK image, expected end-to-end outcome is **FAIL**; do not relabel as full PASS.

Licenses and provenance are inherited unmodified from `LICENSE.md`, `THIRD_PARTY_PROVENANCE.md` and upstream Termux notices. No ownership asserted over Docker, GitHub, Android NDK, Bash, Python, Git or Gradle.

Rollback: close unmerged DRAFT PR or revert only the additive preflight/CI changes; retain all refs and history. `SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM`; `claim_allowed=false`.
