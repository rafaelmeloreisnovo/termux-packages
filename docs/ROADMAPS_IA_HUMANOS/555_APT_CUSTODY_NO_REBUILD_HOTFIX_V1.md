# RAFAELIA 555 — APT Custody No-Rebuild Hotfix

Authorial route: Rafael Melo Reis / RAFCODEPHI. Upstream Termux code and licenses remain attributed to upstream rights holders; this evidence note does not relicense third-party content.

**Status:** SOURCE_PATCH / CI_PENDING at authorship; never promote a source readback to device PASS.
**Route:** `555_00 → 555_10 → 555_20 → 555_30 → 555_90`
**Execution boundary:** no ARM/AArch64 rebuild, no APT publication, no APK installation.

## 00 — Define
Investigate the APT signed bootstrap gate and resolve only the smallest remaining source-side blocker before any costly rerun. Fail closed on ambiguous producer identity. Respect SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM.

## 10 — Source and observed artifacts
- Original failed run: https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/37679999701 (main at `66e3bfc08bf9a8b50f50a0b1bcc759c4f4200657`). ARM/AArch64 bootstrap source-build PASS (205 .deb each); signed-APT old-hook substring false positive.
- Historical APT guard hotfix: PR #134 MERGED, `d623875c07eb5083144f4c982b36d7db254d9741`.
- Successor run: https://github.com/rafaelmeloreisnovo/termux-packages/actions/runs/37696663078 (PR head `77625363a031a081edd1db93d3f7a48e9bfff7f9`). ARM/AArch64 signed bootstrap PASS; DEB custody FAILED; repository materialization and isolated APT SKIPPED.
- Source-build artifact ID `11521200291`; GitHub upload digest `5340b3d05a3257138af7f2513493b8adb5fcd68d1afa4e24c683f3685578af14`.
- Signed-APT artifact ID `11520990883`; GitHub upload digest `5fdc614ac6d4cc517f1a5d12309c02f5eba2a33ac1a08f5d547c9c0a1ea78f8e`.
- Both artifacts were reported not expired and with 2026-11-07 expirations. These digests represent **outer upload archives**, NOT individual .deb or bootstrap hash attestations.

## 20 — First causal falsifier
Job 113049956541 log line 261732: `cannot resolve unique producing recipe for attr-static: []`.
The original emitter `scripts/emit_rafcodephi_package_custody.py` found only direct `packages/<package>/build.sh` or declared `packages/*/<package>.subpackage.sh`.
Termux's `scripts/build/termux_create_debian_subpackages.sh` produces `${TERMUX_PKG_NAME}-static.subpackage.sh` dynamically under the static split contract; `packages/attr/build.sh` exists. The log confirms the output `attr-static` was built.
No direct recipe means **missing mapping**, not a corrupted package.

## 30 — Recomposition strategy and minimal source change
- Add an explicitly scoped generated static producer resolver. Bind `attr-static → packages/attr/build.sh` and other eligible `*-static` outputs only if the canonical split producer script exists and the recipe has not opted out with `TERMUX_PKG_NO_STATICSPLIT=true`.
- Preserve fail-closed unique-producer checks when a direct/explicit/generated candidate conflicts or none exists.
- Each generated mapping emits `producer_resolution=GENERATED_STATIC_SPLIT`, exact recipe blob/SHA-256 and generator blob/SHA-256 plus the output .deb SHA-256. This records how the parent was inferred; the receipt does not claim device runtime.
- Add a synthetic positive/negative test. It requires neither Docker nor compiling packages.
- Repair five malformed `if/${{...}` Bash predicates in `555-artifact-manifold.yml`, which would otherwise break manual artifact replay; add static regression guards.

**Bounded name-only preflight:** 205 unique output package names from existing log; 111 direct, 41 explicit subpackage, 53 auto-static candidates; 0 unresolved. This is a source-tree name-resolution **candidate check** only, not execution of per-DEB custody, binary validation, or complete producer replay.

## 90 — Gates and stop rules
1. Exact-HEAD source syntax + Python synthetic tests must PASS.
2. Artifact-based custody replay must read original preserved source binaries from producer run. Run-specific source SHA must be correctly bound and the graph must reject missing or mismatched producer bytes. **Never rebuild merely to test a map.**
3. Added a separate narrowly allowlisted, manually opted-in read-only replay lane: `.github/workflows/555_20-30_custody_contract.yml` → `workflow_dispatch` → `replay_history=true`. It checks the exact historical run (37696663078), producer commit and GitHub artifact ID/digest before safe extraction. It uses the current audited verifier script against the original checked-out recipe SHA. It uploads only aggregate receipts, not another DEB archive; no heavy producer is invoked.
   Important: merely adding the lane is IMPLEMENTED_UNTESTED_REPLAY. Until its manual run completes with exact-head readback, `CUSTODY_REPLAY_PASS_SCOPED` remains TOKEN_VAZIO.
   The generic 555 artifact-run lane still keeps its distinct main/workflow_dispatch whitelist.
4. APT package indexing, GPG chain, installation, real-device runtime and release remain distinct gates. Publishing requires explicitly authorized production keys and verified runtime.
5. Retain old execution logs, SHA-256 references, and a rollback-able PR; never mutate historical receipts.

**R3** `<F_ok: historical error traced to auto-static mapper; F_gap: full 410 DEB custody and live signed APT still TOKEN_VAZIO; F_next: exact-head CI → no-build per-artifact custody replay → signed APT isolated tests only when evidence authorizes>`.

**Rollback:** revert this independent PR. No original artifact overwritten, no provider check weakened.
