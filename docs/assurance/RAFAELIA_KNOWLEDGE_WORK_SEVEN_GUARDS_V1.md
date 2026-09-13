# RAFAELIA — Seven Knowledge/Work Guards — Termux Packages V1

Repository role: **package/source factory**.

Canonical source: `rafaelmeloreisnovo/Mapa@3a2821d44d555c28cd041ba029cfa940fe3d4c0a`,
`docs/canonical/2026-09-13/CASA_CONHECIMENTO_TRABALHO_V1.md`.

This adapter makes seven guards explicit:

`provenance -> context -> evidence -> contradiction -> uncertainty -> reproduction -> rollback`.

Reconstructibility remains transverse through `reconstruction_pointer`.

## Package-factory boundary

- source is not recipe, artifact, install, or runtime;
- build PASS is not install PASS;
- install PASS is not runtime PASS;
- producer evidence does not become consumer evidence silently;
- licenses remain path/component aware; no single-license flattening;
- physical Android/pkg runtime remains a consumer/device evidence gate;
- `TOKEN_VAZIO != 0 != PASS`;
- `claim_allowed=false` in this adapter.

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
