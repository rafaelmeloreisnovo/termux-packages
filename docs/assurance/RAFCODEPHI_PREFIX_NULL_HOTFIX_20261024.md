# Prefix sentinel and exact-source CI — 2026-10-24

Base/rollback: `07d35d13204fb1403b2d062940e79ac008cf21dc`.
Authority: package producer, draft hotfix requested by user. Public source only.
Governance=DRAFT_NO_PROMOTION; data=PUBLIC_SOURCE; privacy=NO_DEVICE_DATA;
security=FAIL_CLOSED_INPUT. Federated Mapa route=TOKEN_VAZIO; no federated promotion.

Reproduced: NULL at lengths 0, 1, 21 returned 0 (clean). New return contract:
0=clean non-NULL readable span; 1=legacy prefix; 2=invalid NULL, including length 0.
All nonzero values block consumers. Existing valid-input ABI and answers preserved.
Callers still own buffer readability; arbitrary non-NULL pointers cannot be validated
by this pure routine. Current C consumers found: test harness only; Python ZIP/DEB
scanners retain their independent implementation and existing denial behavior.

Evidence: original 6 ZIP/kernel + 4 DEB tests PASS before and after; original build
workflow contract PASS. Added 4 NULL cases, 1,797 deterministic differential span
comparisons, sanitizer C harness. New harness returns 1 against baseline (expected
falsification), 0 after fix. ASan/UBSan PASS with ASAN_OPTIONS=detect_leaks=0;
LeakSanitizer initially failed due to host /proc restrictions, not marked PASS.
Host shared sentinel links -nostdlib, nm -u empty. No runtime imports added.

New lightweight workflow runs tests independently of the 240-minute source-build
job. It consumes rafaelmeloreisnovo/actions/rafcode/verify-source at
`7c03495ebe1d28a11ea419799c4f61427250244f` (draft producer PR #17).
Bash/Git/Python/compiler are explicit host/build-only dependencies. Existing package
runtime dependencies, source-build workflow, licences and ARM32 support unchanged.
Clang unavailable locally: ARM32/ARM64 object builds are CI_PENDING, not local PASS.
Remote exact-head CI pending at publication. APK/build/install/pkg/physical ARM NOT_RUN.

Rollback: revert this scoped commit; retain prior SHA and history. No deletion.
F_ok=local input falsifier closed, prior tested contracts preserved;
F_gap=CI cross objects, signed repository/bootstrap/APK/device chain, human review;
F_next=exact-head CI then real source-built artifacts and physical device receipt.
claim_allowed=false; release_allowed=false.

## Successor S2 — remote feedback and syntax classification

At candidate `6074ed7a0eef5caa301674b2c1d6dd959ee51b82`, GitHub run
40140576934 / job121011272666 completed SUCCESS: exact-source action, 10 ZIP/DEB
tests, ASan/UBSan and both ARM objects with zero undefined symbols.
Runner-reported SHA256 ARM32=c8f22621947704a603ce82249cc8dc2d2256ba7b38d71645e7b703e05f81e9fa;
ARM64=a2f8172dc93a5fe4f9e611d083004d8e20b89a1a95dcfe39e6292aae4af4c59c.
These hashes are read from CI logs, not independently downloaded object hashes.

Separate syntax run40140576904 failed an existing negative fixture: BaseLoader
turns bare `on:` into empty string, classified UNKNOWN rather than MISSING.
Reproduced locally against unchanged baseline validator. S2 maps only empty string
to missing events, preserving rejection. Existing fixture now passes, plus quoted
empty fixture (3 positive/7 negative). Local all-workflow Bash/YAML scan PASS;
555 workflow contract and composer selftest PASS. No trigger allowlist weakened.
Prefix CI path filter now includes these validator/fixture files for successor replay.
S2 remote checks pending; earlier CI PASS is retained only for its original SHA.
Rollback S2: return to candidate6074ed7 (syntax gap returns); full rollback remains base.
