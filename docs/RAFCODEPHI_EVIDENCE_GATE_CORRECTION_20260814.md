# RAFCODEPHI — Evidence-Gate Correction and Cross-Repository Map — 2026-08-14

Status: `DOCUMENTED / FAIL-CLOSED / APPEND-ONLY`  
Claim boundary: `claim_allowed=false`  
Promotion: `false`  
Merge/release: `not authorized by this record`

## Purpose

This record preserves a concrete evidence contradiction discovered during the
RafCodePhi/Termux audit. It does **not** delete or rewrite historical material.
It defines the only safe interpretation of the affected claims until the missing
receipts are produced.

## Authority and primary evidence

| Object | Immutable reference | What it establishes |
|---|---|---|
| Current package baseline | `termux-packages@352db9e927c4b9fb9e7358fb17f909a09982aa2f` | Current `main` at audit time. |
| Pinned bootstrap source | `termux-packages@7b59383c25f7557ba8a29a24f715c5fb5b26cc53` | Signed merge commit; ARM cross-graph compile/link scope only. |
| Delivery contract | `docs/RAFCODEPHI_DELIVERY_CONTRACT_V1.md` blob `b6ede588409acf748c938f35459f32577f56186a` | D0–D8 evidence chain and non-promotion rule. |
| Conflicting historical narrative | `docs/PHASE9_17_DEVICE_VALIDATION.md` blob `1f4f992767b88da50d1af9c4faed5ff38f8eeca9` | Contains unbound device/production assertions. |
| App implementation PR | `termux-app-rafacodephi#358` head `41ae018f475829a03feac7d61aa12d0b30443292` | Draft implementation requiring source-built ARM32+ARM64 bootstraps. |
| Fast app-policy CI | run `31791100646`, `beta-real-bootstrap-contract`, `success` | Static policy, syntax and no-local-bridge regression only. |
| Unresolved CI failure | run `31791100669`, `Vectra-grade Benchmarks`, `failure` | A failing check exists; root-cause log and relation to PR #358 remain unbound in this record. |

## Conflict and controlled interpretation

The delivery contract marks the following gates as not measured:

```text
D3 artifact        = TOKEN_VAZIO
D4 repository      = TOKEN_VAZIO
D5 bootstrap       = TOKEN_VAZIO
D6 Android prefix/shell = TOKEN_VAZIO
D7 pkg/apt transaction  = TOKEN_VAZIO
D8 ARM32 physical       = TOKEN_VAZIO
D8 ARM64 physical       = TOKEN_VAZIO
```

By contrast, `PHASE9_17_DEVICE_VALIDATION.md` contains unbound statements of
physical device PASS, production readiness, performance, thermal behavior and
memory behavior. No source/artifact/APK/device/command/exit-code/hash receipts
were bound to those statements by the evidence inspected here.

Therefore, until independent receipts close D3 through D8, those statements are
classified:

```yaml
epistemic_status: DOCUMENTED_UNVERIFIED_NARRATIVE
claim_allowed: false
release_allowed: false
physical_validation: TOKEN_VAZIO
production_readiness: TOKEN_VAZIO
```

This is a scope correction, not a deletion or claim of falsity. A future
measurement may supersede it only with a successor receipt.

## Current implementation state

`termux-app-rafacodephi#358` has an appropriate bounded design direction:

- source-builds `arm,aarch64` from the exact Termux packages pin;
- requires `source-built-real`, `real-pkg`, and ELF checks;
- blocks bridge and legacy-prefix promotion;
- preserves `claim_allowed=false`;
- preserves physical Android as `TOKEN_VAZIO`.

The fast contract workflow passed, but it is not a substitute for the heavyweight
build, artifact inspection, physical migration or device execution.

## Uploaded-artifact boundary

The locally supplied APK has SHA-256
`e6265a57eb5ca363808488e3b01955958bed93bc0c8a0d281849b363b11027ec`.
It contains ARM32 and ARM64 native libraries, but no repository receipt binds it
to PR #358, the pinned source commit, its signing identity, or a device run.

The supplied ZIP has SHA-256
`53fbfbc52d110d5815024ca851868555d23c3180cd73bb5433dd5f5bade9d93f`
and is a chat-export-shaped archive (`user.json`, `conversations.json`,
`chat.html`), not a bootstrap/package receipt.

Both inputs are therefore retained as unbound local artifacts:

```yaml
artifact_identity: documented_by_sha256
provenance_to_runtime_chain: TOKEN_VAZIO
promotion_allowed: false
```

## Exact closure order

1. **CI root cause:** retrieve the failing `Vectra-grade Benchmarks` log for
   run `31791100669`; bind whether it is caused by #358 or is an independent
   baseline failure.
2. **D3:** produce ARM and AArch64 essential `.deb` artifacts and run the
   fail-closed artifact gate, recording hashes and byte counts.
3. **D4:** generate and independently parse `Packages` and `Release`
   metadata, with an explicit signature/key policy.
4. **D5:** source-build both bootstrap ZIPs, freeze manifest/hash/inventory and
   verify package/profile/ELF constraints.
5. **D6:** inspect the newly built APK and record package ID, ABI, APK hash,
   bootstrap hashes, installed prefix and Bash execution exit/stdout/stderr.
6. **D7:** execute an intentionally governed `pkg/apt` transaction and a
   negative corrupt/unavailable-metadata case.
7. **D8:** collect separate ARM32 and ARM64 physical Android receipts.

## R3

```yaml
F_ok:
  - canonical package/app repository identities were recovered
  - source-built-real policy is present in draft PR #358
  - fast policy gate passed at the audited head
  - contradiction between source/device claim layers is now explicit

F_gap:
  - heavyweight real ARM bootstrap build artifacts and receipts
  - CI root-cause binding for run 31791100669
  - APK inspection bound to the new build
  - physical migration and pkg/apt execution
  - independent ARM32 and ARM64 receipts

F_next:
  - close the CI root-cause binding
  - then execute D3 -> D4 -> D5 -> D6 -> D7 -> D8 without skipping gates
```

## Non-regression invariant

```text
source != artifact != repository != bootstrap != APK != installed runtime != physical proof != release claim
```

Historical PASS language must never silently promote a missing receipt. A closed
`TOKEN_VAZIO` must be recorded as a successor with source, environment,
commands, hashes and falsifier coverage.
