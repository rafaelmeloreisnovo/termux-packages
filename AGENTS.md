# AGENTS.md — RAFAELIA / termux-packages

## Federation entry

This repository is the RAFAELIA **package/source factory**. Enter through indices and contracts, not through a broad repository crawl or remembered prompt.

Sequence:

`bind exact commit → route by index → load open gaps → select deterministic next action → baseline/rollback → mutate narrowly → verify producer/consumer handoff → receipt → append`

Preserve independent axes for knowledge, attention, urgency, operational state and claim gate. `TOKEN_VAZIO`, deferred and ignored-with-reason items remain explicit.

Federation kernel authority: `rafaelmeloreisnovo/RafGitTools:configs/agent-entry-kernel.v1.json` when cross-repository access is available.

## Local role and entry routes

Role: turn pinned sources/build recipes into package/prefix artifacts with reproducible handoff metadata for consumers such as `termux-app-rafacodephi`.

Start with:

- `docs/00-INDEX.md`
- `docs/01-NAVIGATION.md`
- `docs/02-ARCHITECTURE_MAP.md`
- `docs/04-AUDIT_AND_EVOLUTION.md`
- `build-package-rafcodephi.sh`
- the concrete package build recipe/source files touched by the task
- relevant workflow/validator for the exact package boundary

## Package-factory invariants

- Source provenance must be explicit before a package artifact can support a runtime claim.
- A generated property/manifest receipt must match the exact bytes consumed by the build.
- Build success does not prove install/runtime success in `com.termux.rafacodephi`.
- Documentation-only handoff descriptions are not executable handoff evidence.
- REAL/BITRAF/PHASE or other staged outputs must not cross a boundary without a versioned producer/consumer schema once that boundary is used operationally.
- Fail-closed behavior must be explicit; silent continuation across an invalid stage is a gap.
- Package, ABI, source commit, artifact digest and consumer expectation must be linkable in the handoff receipt.
- Physical Android installation/pkg execution remains `TOKEN_VAZIO` until a device receipt exists.

## Current structural gap focus

The documentation audit identifies unresolved handoff questions around staged REAL → BITRAF64 → PHASE9 flows, output schemas, error decisions and end-to-end examples. Treat these as `UNCERTAIN`/`P1` until current executable code proves a stronger state.

Do not promote an old documentation observation directly to a current runtime failure; first bind it to the current commit and inspect the producer/consumer implementation.

## Deterministic work selection

Within an urgency class:

1. repair a broken source/provenance or build blocker;
2. define/validate an upstream output schema before changing a downstream consumer;
3. prefer a reproducible fixture with exact hashes over prose-only clarification;
4. prefer one end-to-end representative package path before multiplying examples;
5. preserve newly discovered gaps in the ledger instead of weakening a validator.

## Evidence receipt

For a handoff-producing action, record at minimum:

- source repository/ref/commit;
- recipe/package identity;
- architecture/ABI;
- input/source digests;
- output artifact digest;
- schema/version at each crossed boundary;
- validator/test result;
- consumer expected identity;
- `F_ok`, `F_gap`, `F_next`;
- rollback reference;
- `claim_allowed` boundary.

## Boundaries

`TOKEN_VAZIO != 0`; urgency is not confidence; `READY_TO_TEST != RESOLVED`; build receipt != install receipt != runtime receipt. Historical observations are append-only and evidence does not silently transfer to a new commit.
