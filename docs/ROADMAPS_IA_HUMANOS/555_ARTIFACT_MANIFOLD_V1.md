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

## Successor: signed APT read-only reconstruction and Bash gate hotfix

Date: 2026-10-08. The existing PR #141 was merged; its original 555 manual workflow
accidentally used GitHub expression delimiters in five Bash predicates. YAML
syntax PASS was not evidence that manually selected options were executable.
The successor fixes only these shell predicates and adds a real fake-provider
execution test in the fast CI gate, with both allowed and forbidden cases.

Historical evidence routes:
- producer run 37679999701, main commit 66e3bfc08bf9a8b50f50a0b1bcc759c4f4200657,
  guard-comment false positive; archive artifacts remained on GitHub;
- producer run 37696663078, same-repo pull_request commit
  77625363a031a081edd1db93d3f7a48e9bfff7f9: signed bootstrap stage PASS,
  per-DEB custody fails at generated attr-static; publication SKIPPED;
- PR #134 guard-comment hotfix merged; PR #142 addresses generated-static custody
  and its exact-head fast source-contract CI passed, independent of full build.

New option: permit_preview defaults false. When explicitly true it permits
read-only observation of a completed, same-repository pull_request run whose
producer YAML is on the allowlist. This NEVER upgrades draft/PR evidence to
main/release, does not rebuild, does not execute downloaded packages, and does
not override source identity, input hashes or `claim_allowed=false`.
Missing producer handoff SHA metadata remains RUN_BOUND_HASHED_UNATTESTED.

Actual run after merge, when authorized:
workflow: .github/workflows/555-artifact-manifold.yml
origin=artifact-run; operation=triage; output_mode=receipt;
producer_run_id=37696663078; permit_preview=true;
artifact_name=rafcodephi-dualarch-source-build-b1d029649351232649f4b1a03eab2084d0fbc4b6

Expected: receipt/triage of a historical FAILED run; not a status transition.
This recipe is NOT an executed 555 run until GitHub readback confirms it.
No automatic rerun, no publication or Android runtime proof.

Validation: Python test scripts/tests/test_rafcodephi_555_workflow_contract.py
checks all Bash run steps with bash -n, plus seven simulated GitHub run
metadata decisions (main, preview denied/allowed, foreign/fork,
unfinished and unknown producer). CI exact HEAD is required before PASS.

R3: F_ok=immutable run metadata + explicit read-only provenance boundary;
F_gap=execution of manual 555 not observed, APT publishing and device
remain TOKEN_VAZIO; F_next=exact-head CI then choose run option and compare
historical artifact identity without rebuilding.
