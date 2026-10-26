# RAFCODEPHI PRoot diagnostic-prefix hotfix — 2026-10-26

Status: `DRAFT/HOLD`; `claim_allowed=false`; no release or device claim.

## Observed predecessor evidence

- Producer commit: `f895a1aa0d0a499614b9650e01ead0bb8434dc81` (PR #161).
- GitHub run/job: `40147024315` / `121030435061`.
- Source-build evidence artifact: `12584063803`, provider digest
  `sha256:686e6a07f2310a8f487b71987aa439df067d349f926bd6a80c9badbe1f4d240a`.
- NDK r29 provisioning and exact-source preflight passed without relaxing the
  repository AppArmor profile.
- The preserved ARM and AArch64 `proot_5.1.107.80` DEBs contain the intended
  RAFCODEPHI prefix in functional paths. Their executable also preserves an
  upstream human-facing diagnostic containing the literal
  `/data/data/com.termux/...`; the bounded bootstrap gate correctly blocked that
  exact legacy marker in `bin/proot`.
- Overall source build remained `BLOCKED`; APT materialization, publication,
  APK import and physical Android execution did not run.

## Scoped delta

The package recipe keeps the upstream URL, version, GPL-2.0 declaration and
pinned source SHA-256 unchanged. A source patch changes only the explanatory
diagnostic string to refer to the host application data directory without a
hard-coded package name. Control flow, ABI, loader selection and path handling
are unchanged. The prefix gate is not weakened.

The failure classifier now recognizes the exact
`SOURCE_REBUILD_REQUIRED_LEGACY_PREFIX` receipt token before the generic Docker
wrapper failure. A fast contract checks that the patch changes exactly one
removed and one added string and does not hard-code the successor package.

The standalone real-bootstrap workflow now reuses the same SHA-256-bound NDK29
provisioner and AppArmor-writable cache already exercised by the signed-APT
workflow. It runs the independent read-only NDK preflight afterward, binds the
verified NDK path into the build command, pins the observed builder image by
digest and uploads both preparation and preflight logs. This closes the known
YML split where one producer path provisioned NDK29 while the other could only
fail on its absence.

## Gates and rollback

Fast source tests can establish only `PASS_SOURCE_ONLY`. Acceptance requires a
new exact-HEAD ARM+AArch64 build where:

1. the source and NDK gates pass;
2. both PRoot DEBs retain their SHA-bound source custody;
3. both ZIP and DEB prefix gates return `PASS_SCOPED`;
4. the isolated signed APT resolution passes without publication;
5. all artifacts and receipts are tied to the same commit and run ID.

Physical device installation, keyboard, shell and `pkg` transactions remain
`TOKEN_VAZIO`. Rollback is the scoped revert of this patch/test/workflow delta;
the predecessor evidence and failure receipt remain append-only.

`F_ok=NDK/AppArmor closure and dual-ABI DEB production observed; F_gap=PRoot diagnostic marker blocks sealed bootstrap; F_next=exact-HEAD dual-ABI rebuild and gate replay.`
