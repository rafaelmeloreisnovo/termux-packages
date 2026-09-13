# RAFAELIA — Seven Knowledge/Work Guards — Termux Packages V1.1

Repository role: **package/source factory**.

Canonical source: `rafaelmeloreisnovo/Mapa@3a2821d44d555c28cd041ba029cfa940fe3d4c0a`,
`docs/canonical/2026-09-13/CASA_CONHECIMENTO_TRABALHO_V1.md`.

Sequence:

`provenance -> context -> evidence -> contradiction -> uncertainty -> reproduction -> rollback`.

Reconstructibility remains transverse through the exact canonical `reconstruction_pointer`.

## V1.1 logic exam / semantic hardening

The V1 structural gate was valid but accepted semantically malformed values where a non-empty field was enough. V1.1 closes that class of bypass while preserving package/source authority.

Fail-closed additions:
- GitHub provenance uses an exact 40-hex commit plus digest-shaped object identity;
- evidence and state vocabularies are closed;
- unknown contradiction/uncertainty/rollback values fail;
- `reproduction=PASS` requires TEST/RUN/RECEIPT/TEST_FIXTURE/CI-class evidence;
- canonical reconstruction pointer is pinned to Mapa;
- material mutation still requires rollback `READY` or `EXECUTED`.

## Package-factory boundary

Source is not recipe, artifact, install, or runtime. Build PASS is not install PASS. Install PASS is not runtime PASS. Producer evidence does not silently become consumer evidence. Licenses remain path/component aware. Physical Android/pkg runtime remains a consumer/device gate. `TOKEN_VAZIO != 0 != PASS`; `claim_allowed=false`.

The validator returns only `BLOCKED` or `READY_FOR_DOMAIN_REVIEW`.
Material mutation requires rollback state `READY` or `EXECUTED`.

## Structural validity is not readiness

`structural_status=PASS` means the envelope is complete enough to evaluate. It does not mean that its source, recipe, artifact, handoff, install, or runtime claim passed.

The synthetic positive fixture remains a validator self-test:

```sh
python3 scripts/validate_knowledge_work_seven_guards.py \
  examples/knowledge-work-seven-guards.example.json
```

The versioned producer snapshot is expected to remain blocked while its named contradictions and uncertainties are open:

```sh
python3 scripts/validate_knowledge_work_seven_guards.py \
  examples/knowledge-work-seven-guards.handoff-20260913.json \
  --expect BLOCKED
```

That command exits zero only when the structure is valid **and** the fail-closed decision is exactly `BLOCKED`. Structural errors still exit nonzero. Changing the expected decision never suppresses a validator error and never changes `claim_allowed=false`.

## Current bounded handoff

`TERMUX-PACKAGES-TO-APP-20260913-001` binds this producer adapter to exact GitHub commits, blobs, CI runs, the active provenance contract, open contradictions, uncertainty probes, reproduction scope, and a non-history-rewriting rollback procedure. Its current status is intentionally `BLOCKED`; a real source-to-recipe-to-artifact receipt and independent consumer/runtime receipts remain `TOKEN_VAZIO`.
