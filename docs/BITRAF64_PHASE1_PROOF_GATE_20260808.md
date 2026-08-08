# BITRAF64 Phase 1 — Proof Gate Hotfix (2026-08-08)

## Status

`OBSERVED_LIMITED`

`claim_allowed=false`

This hotfix reintroduces the useful Phase-1 integration on top of current `main` while refusing claims not supported by the implementation.

## Why this branch exists

The prior Phase-1 artifact described a 33-bit `raf_sig` and a 48-bit packed metadata value, but Manifest V2 exposes only a 32-bit `_reserved` field while preserving a fixed 200-byte ABI. Silent truncation would therefore destroy provenance and make validation dependent on high bits accidentally being zero.

The current implementation keeps Manifest V2 unchanged and adds a sidecar:

- full 33-bit diagnostic fingerprint in `bitraf64_sidecar_v1_t`;
- projected 0..31 coordinate layer;
- 7-bit structural coherence score;
- 32-bit recomputable manifest tag stored in `_reserved`;
- explicit flags that the signature is **not** an ECC proof and the projected layer is **not** real DAG depth.

## Corrected arithmetic

The following claims are now executable regression tests:

- `gcd(6000, 2057) = 1`;
- `gcd(42, 60) = 6`;
- `phi * 15 / (pi * 42) ~= 0.18394`, so it does not derive `0.963999`;
- 53 redundancy bits over 1024 input bytes = `0.0517578125 bits/byte`.

## What Phase 1 proves

1. All 6000 coordinate indices round-trip into `10 x 10 x 10 x 6` coordinates.
2. Wraparound coordinate arithmetic is implemented.
3. Manifest V2 keeps its 200-byte layout.
4. A package payload mutation invalidates the recomputed Phase-1 tag/sidecar pair.
5. The gate fails closed while mathematical/empirical proof obligations remain absent.

## What Phase 1 does NOT prove

- linear transformations are invertible;
- a `33 x 33` syndrome matrix exists or has full rank;
- minimum distance `d_min >= 3`;
- arbitrary single-bit error correction;
- the coordinate projection is equivalent to the real dependency DAG;
- entropy near 5.5 bits/value or white-noise autocorrelation;
- 4-8x SIMD speedup;
- chaotic-but-bounded Fibonacci dynamics;
- the declared `R_corr=0.963999` follows from the published formula.

These remain `TOKEN_VAZIO` / proof obligations.

## Local reference command

```sh
cd core
cc -std=c11 -O2 -Wall -Wextra -Werror \
  bitraf64_integration.c tests/test_bitraf64_integration.c \
  -lm -o /tmp/test_bitraf64
/tmp/test_bitraf64
```

Expected marker:

```text
BITRAF64_PHASE1_PROOF_GATE_PASS
```

## Promotion path

`Phase 1 structural integrity -> GF(2) rank fixtures -> ECC error-injection corpus -> real package DAG mapping -> entropy/autocorrelation measurements -> homogeneous SIMD benchmark -> claim review`

No production/coherence claim is promoted merely because structural tests are green.
