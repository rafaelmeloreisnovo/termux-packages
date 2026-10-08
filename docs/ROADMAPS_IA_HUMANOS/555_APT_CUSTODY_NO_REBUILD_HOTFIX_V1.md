# 555 — Signed APT custody, no-rebuild route V1

**Authorial integration:** Rafael Melo Reis / RAFCODEPHI. This route preserves upstream Termux copyright and licenses. It neither relicenses third-party work nor asserts ownership of upstream source.

**ROUTE:** 555_00 → 555_10 → 555_20 → 555_30 → 555_90
**BOUNDARY:** no compiler, no Docker sourcebuild, no APT publication, no signing key promotion, no APK/device claim.

## 00 — Intent / authority
Investigate failed signed-APT source custody from a completed runner without recreating ARM/AArch64 binaries. Only source and evidence gates are in scope. Runtime, release, user installation and provider trust remain separate.

## 10 — Exact historical identifiers
- Original signed APT run: https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/37679999701 ; commit `66e3bfc08bf9a8b50f50a0b1bcc759c4f4200657`; ARM and AArch64 sourcebuild reached PASS with 205 .deb each; signed bootstrap failed on inactive APT guard comment (false positive).
- Guard fix: PR #134 merged, source change compared active APT directives rather than a comment substring; commit `d623875c07eb5083144f4c982b36d7db254d9741`.
- Successor failing run: https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/37696663078 ; job `113049956541`; step 6 sourcebuild SUCCESS, step 7 per-DEB custody FAILURE, remaining repository/isolated APT gate SKIPPED.
- Successor checkout PR merge-ref in job log: `b1d029649351232649f4b1a03eab2084d0fbc4b6` (do not substitute for source commit).
- Source-build upload artifact: ID `11521200291`, outer archive SHA256 `5340b3d05a3257138af7f2513493b8adb5fcd68d1afa4e24c683f3685578af14`.
- Signed-APT upload artifact: ID `11520990883`, outer archive SHA256 `5fdc614ac6d4cc517f1a5d12309c02f5eba2a33ac1a08f5d547c9c0a1ea78f8e`.
- GitHub listed both uploads as not expired, expiration 2026-11-07. The outer digest is **not** individual .deb attestation.

## 20 — Root-cause candidate and falsifier
First failure, job log line 261732: `cannot resolve unique producing recipe for attr-static: []`.
Historical custody mapper recognized only durable `packages/<name>/build.sh` or `packages/*/<name>.subpackage.sh`. Canonical script `scripts/build/termux_create_debian_subpackages.sh` dynamically creates `${TERMUX_PKG_NAME}-static.subpackage.sh`, so `attr-static` is legitimately absent as a persistent file.
Falsifier: a synthetic static split with parent recipe + generator resolves; a parent with `TERMUX_PKG_NO_STATICSPLIT=true`, missing generator, or ambiguous explicit producers must FAIL.
This establishes a **producer mapping issue**, not corruption/integrity or valid Android runtime.

## 30 — Minimal correction consolidated in PR #142
- `scripts/emit_rafcodephi_package_custody.py`: generated `-static` resolving with parent recipe and canonical generator; explicit/ambiguous cases still fail closed. Check opt-out. Store `producer_mapping_method`, .deb hash, recipe and generator Git blobs and SHA256.
- `.github/workflows/rafcodephi-publish-dev-apt.yml`: hash sidecar verified from its proper directory, `source_commit` bound to actual `GITHUB_SHA` in the quoted Python heredoc.
- `.github/workflows/555-artifact-manifold.yml`: correct 5 malformed shell predicates, preserve manual source-only default and prior artifact read-only path.
- Synthetic contract tests for mapping, ambiguity, static opt-out, generator, shell predicates and sidecar/SHA regression. Source-test success does NOT retroactively change the historical failed run.

**No-breakage gate:** do not enable unrestricted replay of PR-branch artifacts in a main-only replay lane merely for convenience. A separately authorized, read-only historical lane is necessary for any such replay. Never rebuild on a provenance-only gap.

## 90 — Evidence and stop decision
The original source-build and uploaded artifacts remain immutable. PR exact-head contract CI is necessary but not evidence of a new full signed repository.
After code review, next narrow task: archive custody replay using historical exact source SHA and bytes from the correct run, with per-.deb hashes verified. Rebuilding the 410 packages for a mapping correction is **not** part of this route.
Stop with `TOKEN_VAZIO` for missing item-level archive hashes, subsequent APT index/signature validation, real Android install and runtime claims.
`SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM`.
`CLAIM_ALLOWED=false; DEVICE_RUNTIME=TOKEN_VAZIO; RELEASE_ALLOWED=false`.

## Relationship of PRs
- PR #142 is the consolidation target (fix producer mapping + sidecar + SHA + 555 predicates).
- PR #143 is overlapping evidence; **do not merge both blindly**. Compare exact trees and preserve any unique changes before archiving the redundant candidate.

**R3** = <F_ok: source-failure mapped and bounded synthetic gates; F_gap: exact historical per-DEB replay and signed APT runtime; F_next: exact-head CI → review → read-only replay → receipt>.

## μDELTA — 555 historical replay lane (post-PR #142)
Source: PR #142 was merged at `a7be64df440d0b67373a1530562dc00414053eb4`, governing the production mapper/contract. The old overlapping PR #143 is not a safe merge target.

New independent workflow: `.github/workflows/555_20-30_custody_contract.yml`.
- `pull_request`: only Python/source fixtures, shell-contract and syntax checks; no ARM/AArch64 source builder.
- `workflow_dispatch` default: only source tests.
- `workflow_dispatch replay_history=true`: explicit opt-in for signed APT historical run 37696663078, exact source commit 77625363a031a081edd1db93d3f7a48e9bfff7f9, source artifact 11521200291 and outer provider digest 5340b3d05a3257138af7f2513493b8adb5fcd68d1afa4e24c683f3685578af14. Requires run metadata + artifact readback.
- The replay checks the archived TAR SHA, safely extracts the preserved .deb payload, validates both SHA256SUMS sets and calls the PR#142 canonical custody emitter with the original recipe Git commit. Only JSON receipts and hash references are uploaded.
- Neither this workflow nor its tests publishes the APT repository, compiles source, installs APKs, or verifies physical Android execution.
- `IMPLEMENTED_UNTESTED_REPLAY` remains until a manual exact-head replay with receipt exists. Any mismatch is BLOCKED and historical artifacts remain unchanged.
- If the historical artifact expires, remain TOKEN_VAZIO; do not recreate bytes or accept a substitute based on matching filenames.

Rollback: revert only this isolated replay workflow/document pointer. Source mapper remains governed by PR #142.
