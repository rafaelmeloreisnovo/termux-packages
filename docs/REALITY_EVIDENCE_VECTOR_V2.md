# Reality Evidence Vector V2

Status: `DRAFT_GOVERNANCE_CONTRACT`  
Default: `claim_allowed=false`

## Why V2 exists

The repository contains multiple kinds of truth that must not be collapsed:

- source code that exists;
- code that compiles;
- code that has executed;
- real I/O with incomplete semantics;
- prototypes;
- simulations;
- security-sensitive names backed by toy implementations;
- architecture tables containing both observed identity and nominal capability;
- device/CI/replication evidence that has not yet been observed.

V1 used lexical heuristics to classify `core/*.c`. That is useful for discovery, but a heuristic is not proof. V2 makes the classification source explicit: the registry is authoritative, heuristics are risk signals only, and every unregistered module becomes `TOKEN_VAZIO`.

## Evidence vector

Every governed module carries:

```text
V = <code, build, runtime, test, ci, device, portability, security, provenance>
```

Each axis must be one of:

```text
PASS
FAIL
BLOCKED
OBSERVED
OBSERVED_LIMITED
TOKEN_VAZIO
NOT_APPLICABLE
```

Classification is separate from the evidence axes:

```text
VERIFIED_LIMITED
OBSERVED_LIMITED
PROTOTYPE
SIMULATED
STUB
TOKEN_VAZIO
REFUTED
OUT_OF_DOMAIN
```

`TOKEN_VAZIO` means the evidence required by that axis is absent or not yet classified. It is not PASS, FAIL, zero or proof.

## Promotion rule

A module must not set `claim_allowed=true` unless the minimum promotion axes are all `PASS`:

```text
code + build + runtime + test + provenance
```

Even then, the claim must be scoped to the axes actually proven. Device, portability or security claims require their corresponding evidence independently.

## Priorities

### P0 — claim/safety boundary

- `metrics_producer.c`: real graph metrics must not imply product readiness.
- `real_contract.c`: minimal textual JSON extraction needs adversarial coverage or a bounded parser.
- `real_arch.c`: split observed architecture identity from nominal/runtime capabilities.
- `crypto_ed25519.c`: toy/simulated; security axis `FAIL`.
- `crypto_chacha20.c`: toy/simulated; security axis `FAIL`.
- `crypto_pqc.c`: toy/simulated; security axis `FAIL`.

### P1 — semantic/runtime closure

- scanner: prove root coverage and propagate scan failures;
- parser: type unresolved Bash semantics and compare against a controlled oracle;
- DAG: providers/alternatives/conditionals + SCC cycles + allocation failure propagation;
- provenance: bind source/artifact/environment hashes and independent replay;
- freestanding: x86_64 proof must not be widened to ARM without separate backends/receipts;
- RPC/distributed: framing, strict parsing, authenticated identity, real assignments and homogeneous benchmarks.

### P2 — unregistered modules

Every `core/*.c` without a V2 registry entry is automatically `TOKEN_VAZIO`. The next gate is to classify its semantics and collect evidence. This prevents ignored/forgotten modules from silently inheriting a global REAL label.

## Governance baseline invariant

The regression baseline is itself evidence. Therefore:

```text
baseline missing       -> FAIL/BLOCKED
baseline invalid JSON  -> FAIL/BLOCKED
baseline status !=REAL -> FAIL/BLOCKED
baseline TOKEN_VAZIO   -> FAIL/BLOCKED
required field absent  -> FAIL/BLOCKED
```

A missing baseline is no longer a warning that silently skips the regression gate.

## Commands

Generate the V2 audit report:

```bash
python3 core/audit_reality_v2.py
```

Run the strict P0/conflict gate:

```bash
python3 core/audit_reality_v2.py --strict
```

Run unit tests:

```bash
python3 -m unittest core/tests/test_reality_audit_v2.py
```

The generated report path defaults to:

```text
core/tests/fixtures/reality_audit_v2.json
```

The generated report is execution evidence and should only be committed when its source commit/environment are intentionally frozen.

## Scope boundary

A green `pkg_metrics` governance gate means only that the metrics artifact satisfies its declared contract and regression policy. It does not prove:

- every Termux package builds;
- `.deb` repository closure;
- bootstrap completeness;
- APK installation/runtime;
- `pkg`/APT network operation;
- ARMv7/ARM64 physical execution;
- security;
- distributed speedup;
- cross-architecture compatibility;
- independent reproduction.

Those remain separate gates and must produce their own receipts.

## R3

```text
F_ok:
  explicit evidence vector + typed TOKEN_VAZIO + P0/P1/P2 + fail-closed baseline

F_gap:
  current branch still needs execution receipts; most core modules remain unregistered by design

F_next:
  run V2 audit/tests, freeze report with commit/environment provenance, then close P0 in order:
  toy crypto quarantine -> JSON adversarial contract -> architecture runtime probes -> metrics coverage semantics
```
