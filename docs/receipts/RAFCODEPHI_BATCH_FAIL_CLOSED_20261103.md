# Candidate APT fail-closed hotfix — 2026-11-03

μID: MU-TERMUX-BATCH-FAIL-CLOSED-20261103-S1
Parent: producer main 5be0f29ae21e917e1eaeb653982056ab1fb5ecf8.
Repository: rafaelmeloreisnovo/termux-packagesMy (old termux-packages URL redirects).
Status: candidate, local scoped tests; hosted CI pending at commit creation.

## Decision and authority

Package/source factory owns this candidate workflow and its result receipts.
User requested surgical workflow corrections with reversible tests. Governed by
root AGENTS.md at baseline, existing Apache-2.0 infrastructure license, and
Mapa/governance/DMAIC_ONE_PASS_BRANCHING_V1.md. Public source/test fixtures only;
no credentials, source recipe, package publisher, release or consumer changed.
Intention: prevent a failed APT operation or incomplete materialization producing
PASS evidence. CTQ: injected failure never returns success; successful fixtures
retain their existing result schema. Scope is failure propagation, not a new
freestanding runtime. Unknown device and performance results remain TOKEN_VAZIO.

## Baseline falsification

1. Executing the exact APT Bash body with offline apt-get update returning 73
   produced process exit 0. Parent set +e reached the subshell; later commands
   masked the earlier failure. Fixture bytes deliberately are not a real DEB.
2. Executing the bundle body with pre-existing directories and producer outcome
   failure still produced state=PASS. Directory existence was the sole gate.
3. Code inspection also showed the APT path ignoring tee's nonzero exit.

Historical run 41277236154 returned jobs=[] in this session. Its root cause is
TOKEN_VAZIO; the reproduced defects above are not asserted to explain that run.

## Correction and gate

Restore errexit inside the APT subshell, capture both pipeline exit statuses in
one assignment, propagate a failed logger when the APT body succeeds. Give the
materialization step an ID and pass its actual outcome through an environment
binding to the bundle gate. Require exact success plus existing directories.
Keep failure uploads, read-only permissions, manual expensive build and schema.
Add offline executable tests and one bounded routine with README/license links.

Host: Linux x86_64, GNU Bash 5.2.21, Python 3.12.14, PyYAML 6.0.3 (existing syntax
test dependency only). No network used by runtime tests. Test children timeout
10 seconds; source-contract workflow timeout 10 minutes. Not dual hardware
watchdogs. No speed, cost, NUMA or cross-ABI runtime claim.

Commands/results:

- bash scripts/rafcodephi/batch-guard/check.sh: 18/18 PASS_SCOPED.
- python3 scripts/tests/test_rafcodephi_batch_pipeline_falsifiers.py: 8/8 PASS_SCOPED.
- python3 scripts/tests/test_rafcodephi_workflow_syntax.py: 47 workflows / 259 run
  bodies parsed PASS; embedded Bash syntax verified, not full Actions semantics.
- bash -n scripts/rafcodephi/batch-guard/check.sh: PASS.
- git diff --check: PASS.

The 18 cases cover two architecture metadata fixtures with success, apt update,
apt-cache, install, dpkg and tee failures; five producer outcomes; and stale
bundle cleanup after a previously successful attempt. No real APT, GPG signing,
Docker build, ELF/DEX execution or physical Android occurred.

## R3 / rollback

F_ok: two baseline false-positive paths reproduced; local corrected failure paths
and existing structural contract pass. No added application runtime dependencies.
F_gap: remote exact-head CI pending; real ARM32/ARM64 builds/signatures/device,
independent runtime watchdog/failover, low-level migration and performance remain
NOT_RUN. Existing publisher and other pipeline logging paths require separate
source-scoped review; these tests do not certify them.
F_next: exact-head hosted CI, then a separate artifact-bound build contract before
any release/runtime promotion. claim_allowed=false; release_allowed=false.
Rollback: before merge close candidate PR; after merge revert only this hotfix
commit. Re-run the fixtures and preserve this receipt; reverting reintroduces the
baseline defects. No reset, deletion of source history or automatic merge.

## SHA-256 of candidate files (receipt excluded to avoid self-reference)

- `.github/workflows/rafcodephi-batch-pipeline.yml`: `4845464799654ac23f9a4958bf2cf97e72a5378315e913a4433d3e4b8afeeb41`
- `scripts/tests/test_rafcodephi_batch_runtime.py`: `6b50a501c9f6864a899170c3576eb9be5a4158c4e7202d4c83b7ed5e095b5c8a`
- `scripts/rafcodephi/batch-guard/check.sh`: `87fa605ed57bd8e7d7aa0e75378651e2fafcbda84f156318ab6ae2f77ea6c10b`
- `scripts/rafcodephi/batch-guard/README.md`: `7adf468516df925e4e0787a7257177b74560dfa6159b7f3daff43c54992357c0`
- `scripts/rafcodephi/batch-guard/LICENSE.md`: `05a8d413740d7a32679b0bb9f2c240c1c6d325ed7fea31ee07366b3d443efad6`
- `001_PONTEIRO/README.md`: `5540ca5ba0f5da34f66bd500ab13caa5131cfe2ae0887c9a475919b79dc1ab37`
- `docs/00-INDEX.md`: `b060e90f5f6ba5d43fe4149cb4d0f593faf2503a33556c7172e701c6e47deb4e`
