# RAFCODEΦ BITRAF64 Manifest Bridge — Experimental V1

**State:** `EXPERIMENTAL / OBSERVED_LIMITED`  
**claim_allowed:** `false`  
**production_ready:** `false`

This directory introduces a **namespaced experimental bridge**, not a claim that the Termux package DAG is toroidal and not a replacement for existing package/artifact integrity mechanisms.

## Boundary invariants

1. SHA-256/BLAKE3/package-manager integrity remains authoritative where already required.
2. BITRAF64 metadata is additive and optional until independently validated.
3. Missing BITRAF64 evidence is `TOKEN_VAZIO`, never inferred as PASS.
4. `gcd(6000,2057)=1`; no structural package alignment is inferred from the previously reported value 16.
5. `raf_sig`, ECC correction, transform invertibility, entropy/whiteness and SIMD speedup are blocked until their own receipts exist.
6. A green CI process may report that the validator executed correctly while the product/promotion gate remains blocked.

## Bridge record V1

A JSON record consumed by `validate_bitraf64_manifest_bridge.py` has this minimal structure:

```json
{
  "schema": "rafcodephi.bitraf64.manifest-bridge.v1",
  "package": "example",
  "artifact_sha256": "<64 lowercase hex>",
  "bitraf64": {
    "state": "TOKEN_VAZIO",
    "claim_allowed": false,
    "coordinate": null,
    "raf_sig": null,
    "rank_receipt": null,
    "ecc_receipt": null,
    "benchmark_receipt": null
  }
}
```

`coordinate`, when present, is `[i,j,k,f]` with `i,j,k∈[0,9]` and `f∈[0,5]`. This validates an address contract only; it does not prove a fractal dimension, package-DAG isomorphism or toroidal runtime behavior.

## Promotion contract

`claim_allowed=true` is rejected unless all three evidence classes below are materialized as non-empty receipt references:

- `rank_receipt`: actual `GF(2)` transforms, full-rank/zero-kernel and round-trip evidence;
- `ecc_receipt`: unique single-error syndrome / `d_min≥3` evidence and exhaustive error injection for the declared protected word;
- `benchmark_receipt`: homogeneous physical-target measurements if performance claims are made.

Even with those fields present, this bridge only checks **manifest completeness**. It does not independently validate the scientific contents of the receipts; that promotion belongs to the RLL/Mapa evidence gate.

## Canonical scientific/governance anchors

- RLL branch: `audit/bitraf64-toroidal-coherence-20260808`
- Mapa branch: `audit/bitraf64-evidence-gate-20260808`

## Execution

```sh
python3 scripts/rafcodephi/bitraf64/validate_bitraf64_manifest_bridge.py manifest.json
python3 scripts/rafcodephi/bitraf64/validate_bitraf64_manifest_bridge.py --strict manifest.json
```

Exit states:

- `0`: record is structurally valid; this does **not** mean production PASS.
- `1`: malformed/contradictory record.
- `2`: strict mode and promotion remains blocked (`TOKEN_VAZIO` / `claim_allowed=false`).
