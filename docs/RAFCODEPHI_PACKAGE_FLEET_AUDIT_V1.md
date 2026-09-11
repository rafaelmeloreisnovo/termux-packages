# RAFCODEPHI Package Fleet Audit V1

Date: 2026-09-10
Authority: `rafaelmeloreisnovo/termux-packages`
Scope: package recipe fleet (`packages/`, `root-packages/`, `x11-packages/`)

## Why this exists

Package work must not stop after the first visible defect. A single recipe failure can be only one symptom of a fleet-wide validator, metadata, branch, dependency, source, architecture, or CI defect.

The repository's standard `Packages` workflow currently builds its lint input from `built_${repo}_packages.txt`; that is useful for touched/built package paths but is not a proof that the complete recipe fleet was inspected in the same run.

The legacy `scripts/lint-packages.sh` also currently contains fleet-wide tooling gaps that must remain visible:

- `check_version()` is bypassed by an early temporary `return`.
- base calculation contains an `origin/master` assumption while this fork's canonical branch is `main`.
- the literal `TERMUX_PKG_REVISION` range expression uses `OR`, allowing nearly every integer through that branch.
- the SHA-256 pattern is not anchored at both ends.

V1 does not erase these facts. It adds an independent whole-fleet static layer that compensates for the two literal validators and records the remaining tooling debt explicitly.

## Outputs

Every run emits:

- `packages.tsv`: every discovered `build.sh` and its channel/package identity.
- `findings.tsv`: one normalized finding per line with severity, stable code, recipe, and detail.
- `summary.txt`: counts, current Git SHA, evidence hashes, claim scope, and gaps.

The GitHub workflow uploads these three files as a single audit artifact.

## Stable finding families

The scanner groups symptoms into cause families instead of stopping at the first package:

- syntax/layout: `BASH_SYNTAX`, `CRLF`, `NO_FINAL_NEWLINE`, `EMPTY_RECIPE`, `EXECUTABLE_RECIPE`
- package metadata: `FIELD_NOT_LITERAL_*`, `DESCRIPTION_TOO_LONG`
- versioning: `VERSION_SYNTAX`, `REVISION_RANGE`
- source integrity: `SHA256_LITERAL`
- repository topology: `DUPLICATE_PACKAGE_NAME`
- validator/tooling: `TOOLING_VERSION_GATE_DISABLED`, `TOOLING_BASE_BRANCH_HARDCODED`, `TOOLING_REVISION_RANGE_LOGIC`, `TOOLING_SHA256_UNANCHORED`

This is intentionally many-to-one: thousands of package symptoms can map to one cause family.

## Claims and gates

`SOURCE != ARTEFACT != EXECUTION != EVIDENCE != CLAIM`

A successful static fleet scan proves only the static scope it actually inspected. It does **not** prove source availability, extraction safety, patch application, dependency closure, cross-architecture compilation, install/remove behavior, device runtime, or reproducibility.

Therefore:

- static scan with zero `ERROR`: `EVIDENCED_STATIC`
- static scan with `ERROR`: `REFUTED_STATIC`
- physical Android execution: `TOKEN_VAZIO` until a device receipt exists
- full build matrix: `TOKEN_VAZIO` until executed and evidenced

`claim_allowed=false` for full-fleet runtime/build correctness until the corresponding gates close.

## Operating sequence

1. Run the whole-fleet report once.
2. Sort `findings.tsv` by finding code/count.
3. Fix the highest-frequency cause family, not the first package in lexical order.
4. Rerun the whole fleet.
5. Record the delta: removed findings, new findings, unchanged gaps.
6. Promote a cause family to a blocking CI gate only after baseline debt is understood.

This keeps the first run diagnostic instead of making an unknown historical backlog silently block every unrelated pull request.

## μWRITE receipt

`μID=PKG-FLEET-V1-20260910 | source=main@80a1da4ac51fcb74afbc36df104794f342da946b | kind=control-plane | Δsummary=whole-fleet recipe discovery + normalized cause-family findings + CI artifact | routes=L/P/C/R/I/E/A | evidence=branch+workflow+artifact-after-run | gap=legacy version gate + base-branch assumption + full build/device matrix | next=run report, rank finding families, batch-fix highest-frequency family | claim_allowed=false`

## R3

- `F_ok`: whole-fleet discovery and normalized evidence path are implemented on the audit branch.
- `F_gap`: actual fleet counts/findings require CI execution; runtime/device/full-build remain `TOKEN_VAZIO`.
- `F_next`: execute report, rank by frequency, then patch whole cause families in descending impact order.
