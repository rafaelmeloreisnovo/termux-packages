# 555_20_30 — APT custody auto-static producer receipt V1

AUTHOR: RAFAEL MELO REIS / RAFCODEPHI
STATUS: SOURCE_PATCH_PENDING_EXACT_HEAD_CI
ROUTE: 00 → 10 → 20 → 30 → 90
MODE: NO_REBUILD | SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM

## 00 INTENT
Resume the signed APT chain after an existing 410-.deb dual-architecture source build, without rebuilding or weakening trust guards.

## 10 OBSERVED SOURCES
- Original failed provider run [37679999701](https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/37679999701): both 205-.deb source sets PASS; APT hook substring false-positive blocked downstream. PR #134 merged a scoped guard fix.
- Successor provider run [37696663078](https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/37696663078), job 113049956541: source build and bootstrap PASS; custody FAIL with `cannot resolve unique producing recipe for attr-static: []`; signed repository build/resolution/publish SKIPPED.
- Preserved artifacts from successor: source-build artifact 11521200291 (SHA256 digest 5340b3d05a3257138af7f2513493b8adb5fcd68d1afa4e24c683f3685578af14); signed-apt artifact 11520990883 (digest 5fdc614ac6d4cc517f1a5d12309c02f5eba2a33ac1a08f5d547c9c0a1ea78f8e). Provider reports available as of 2026-10-08; neither artifact is publication proof.

## 20 ROOT CAUSE
`scripts/build/termux_create_debian_subpackages.sh` generates `${TERMUX_PKG_NAME}-static.subpackage.sh` inside `TERMUX_PKG_TMPDIR` on an active static library split. Existing `scripts/emit_rafcodephi_package_custody.py --auto-map` only discovers `packages/<output>/build.sh` or checked-in `packages/*/<output>.subpackage.sh`. Generated `attr-static` cannot be mapped, even though its generating source rule is present.

## 30 MINIMAL CHANGE
A source-bound `dynamic_static_producer` candidate is accepted only if:
1. The parent and its derived -static .deb are both in the immutable package set.
2. Parent build.sh exists; the Termux generator contains the exact dynamic split rule.
3. Parent does not explicitly disable static splitting.
4. Both DEBs have equal version, output description is `Static libraries for <parent>`, and `Depends` pins the exact parent version.
5. Candidate set remains UNIQUE, otherwise FAIL-CLOSED.

A fast synthetic fixture checks positive static resolution and negative mismatches (version, parent, disabled split, description, dependency, generator drift). It does not run a full compiler or publish any repository.

## 90 RECEIPT BOUNDARIES
Source delta is NOT validated build evidence until exact-head CI. Even a passing custody fixture is NOT proof of resolving all packages from 410 .debs. Runtime, persistent signing key, APT resolution on Android and APK install remain TOKEN_VAZIO. No existing build or artifact is modified; no job rerun requested.

ROLLBACK: revert this isolated resolver/test change; preserve predecessor logs/artifacts and supersede documentation append-only.
NEXT: exact-head lightweight contract -> readback provider CI -> use existing artifact with a separate offline custody verifier/replay; only if all package identities pass, consider downstream repository metadata and isolated signature test, never auto-publish.
