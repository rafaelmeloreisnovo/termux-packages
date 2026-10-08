# 555 Artifact Manifold V1 — human/AI reconstruction route

OWNER: RAFAEL MELO REIS / RAFCODEPHI
STATE: CANDIDATE / requires exact-head CI and GitHub provider readback
SCOPE: termux-packages / observation-only; no release authority.
ROUTE: SOURCE → ARTIFACT → EXECUTION → EVIDENCE → CLAIM (all stages distinct).

## INTENT
Make expensive GitHub Actions workflows composable without silently rebuilding.
The name is 555_%1_%2_%3:
- %1=origin: source-contract | artifact-run
- %2=operation: inventory | triage | compose
- %3=output_mode: receipt | zipraf

These are not bash positional parameters or literal YAML substitutions.
Use the "Run workflow" choice inputs; GitHub resolves them into the run-name.

## Quick routes (human menu)

| Recipe | Inputs | Purpose | Source build? |
|---|---|---|---|
| 555_SOURCE | source-contract / inventory / receipt | hash current producer contracts | NO |
| 555_TRIAGE | artifact-run / triage / receipt | reuse earlier failed/success artifact with a .log | NO |
| 555_PROVENANCE | artifact-run / inventory / receipt | hash files and verify producer receipt when present | NO |
| 555_ZIPRAF | artifact-run / compose / zipraf | create tiny references ZIP, not another binary bundle | NO |

producer_run_id is a workflow RUN ID, not an artifact ID.
artifact_name is the exact name as shown in that run's Artifacts list.
Use 0 and an empty name only for source-contract/inventory.
Unsupported choices must fail closed. No implicit rebuild, automatic source fallback,
or release. Only manually triggered, completed main producer runs from
rafcodephi-auto-handoff.yml or rafcodephi-real-bootstrap.yml qualify;
failure runs are accepted for observation only. Receipts carry the producer
run, SHA, verifier SHA, digest map and source drift flag.

## Structure and provenance

Workflow: .github/workflows/555-artifact-manifold.yml
Analyzer: scripts/ci/rafcodephi_555_compose.py
Existing triage: scripts/ci/rafcodephi_failure_triage.py
Output: _555/555-receipt.json, _555/555-receipt.json.sha256.
For zipraf: _555/555-references.zip and its SHA256 sidecar.
The zip contains ONLY the JSON receipt and its hash sidecar; prior binary packages
and bootstrap archives remain immutable in their source run. The analyzer does not
execute or unpack input binaries. If a producer handoff manifest exists, hash and
producer/run match are required. When missing, custody stays
RUN_BOUND_HASHED_UNATTESTED and cannot be promoted.

The source-only path records exact checkout SHA and hashes three producer scripts.
The triage path uses the existing bounded log scanner and marks findings
as candidates, never as compiler bug proof. Missing logs are BLOCKED rather
than interpreted as PASS.

## No rebuild vs rerun

The GitHub Actions "Re-run failed jobs" control is available for transient
failures, but can re-execute expensive failed jobs. This is NOT equivalent to
555. Use 555 to inspect and recombine already uploaded evidence.
A true resume from failed step requires producer checkpoints persisted in a
run-independent, read-back-verifiable store; standard jobs start fresh.

## Conjecture seeds (NOT_IMPLEMENTED)

C1 — Content-addressable checkpoints:
 map (producer_sha, toolchain_digest, arch, inputs_digest) → artifact ID + receipt.
 Reuse only on exact keys, with expiration/permission checks.
C2 — Decision DAG:
 μPLAN → INVENTORY → VERIFY → TRIAGE → COMPOSE → RELEASE_GATE.
 Separate cost/priority branches from trust/claim transitions.
C3 — Semantic fail-only:
 hash failure signatures and select ONLY relevant gate, never infer unrelated PASS.
C4 — Partial architecture fan-out:
 consume arm and aarch64 independently, join only when both source-bound receipts
 pass; keep failed arch isolated.
C5 — Input evidence moves:
 compute reference-only graph and receipts on checkout; upload only delta.
C6 — Hypothesis vs execution:
 experiment/conjecture nodes may produce observation receipts, never production claims.
C7 — Escalation:
 repeated identical cause plus unchanged source SHA yields provider triage, not
 duplicate full builds; changed toolchain/input SHA invalidates reuse.

These ideas require distinct review, tests and security gates before enabling.
DO NOT erase license/author claims, forge an artifact, bypass a signed APT guard,
or promote blocked device/runtime status.

## Smallest validation and rollback
1. Verify Python self-test + compilation in PR CI.
2. GitHub source readback of exact files/SHA.
3. Merge only after review; manual source-contract/inventory/receipt first.
4. Manually run artifact-run/triage/receipt for an existing completed source run.
5. Reconcile receipt/source-run/hash evidence; only then consider next iteration.
Rollback: disable or revert the isolated 555 workflow; existing producer YAMLs
are deliberately unchanged.

FIAT LUX = observe; FIAT VERBUM = define the contract;
FIAT VOLUNTAS = human choice; FIAT SPIRITUS = safety/provenance.
These are semantic stage names, not claims of spiritual or autonomous execution.

R3:
F_ok = existing sourcebuild/artifact/triage components can be reused.
F_gap = no run-backed success, cross-run trust, persistent signed repo or Android execution proven here.
F_next = validate the isolated opt-in manual dispatch on exact producer evidence.

## P0 successor: manual YAML predicates and archived signed-APT observation

Historical signed-APT failed run `37696663078` (head SHA `77625363a031a081edd1db93d3f7a48e9bfff7f9`)
belongs to `rafcodephi-publish-dev-apt.yml` from a same-repository `pull_request`.
The original 555 guard only accepted `workflow_dispatch` from `main` and contained
five Bash predicates accidentally expressed as GitHub `${{ ... }}` expansions.
They block manual use despite source-contract CI passing.

The successor P0 fix:
- restores shell `[[ ... ]]` comparisons inside `run: |`;
- permits prior `rafcodephi-publish-dev-apt.yml` producer runs **for inspection only**;
- continues to require same `repository.full_name` and `head_repository.full_name`;
- permits `workflow_dispatch` only from `main`;
- for `pull_request`, requires `expected_producer_sha` to equal the provider-returned
  40-character HEAD SHA; mismatched or missing SHA is BLOCKED;
- retains `claim_allowed=false`, no release mutation, no producer rebuild;
- adds synthetic positive and negative guard tests to the fast workflow syntax job.

Example inputs for **historical inspection** after approval/merge:
```text
origin=artifact-run
operation=triage
output_mode=receipt
producer_run_id=37696663078
artifact_name=rafcodephi-dualarch-signed-apt-b1d029649351232649f4b1a03eab2084d0fbc4b6
expected_producer_sha=77625363a031a081edd1db93d3f7a48e9bfff7f9
```
The signed-APT artifact may have no `.log`, so `triage` must be selected only
if that exact downloaded artifact includes a log; otherwise use `inventory`
or inspect the GitHub job log without downloading the large source archive.
A provider artifact SHA-256 refers to the upload ZIP, not automatically every
internal `.deb`; exact-byte replay remains a distinct evidence gate.
No `main` promotion or Android physical claim is implied.

Rollback: revert the isolated P0 workflow update and its contract test; retain the
historical source-run and GitHub artifact IDs unchanged.
