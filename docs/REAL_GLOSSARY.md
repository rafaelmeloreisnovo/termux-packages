# REAL Glossary — status tokens & sentinels

A short authoritative reference for every status/sentinel string the
REAL layer emits or consumes. Consumers use these strings to decide
whether an artifact can be trusted and for what purpose.

---

## Status tokens (used in `"status"` fields)

| Token | Meaning | Example emitter |
|---|---|---|
| `REAL` | Values are derived from a real syscall, file read, or hash. Fully within contract scope. | `metrics-producer` (pkg_metrics/1.0.0), all receipts, ledger entries |
| `OBSERVED` | Values are read from the running host at that moment. Supersedes any nominal reference. | `arch-probe` (arch_capability/1.0.0) |
| `OBSERVED_LIMITED` | Static catalog. Identifiers are stable; per-item property fields are NOMINAL references and must not be promoted to runtime evidence without a separate observation. | `arch-detect --json` (real_arch table) |
| `SIMULATED` | Values come from a simulation, mock, or synthetic workload — not from real hardware / real files. Excluded from the REAL layer. | legacy `build_orchestrator_v2`, `gpu_integration`, etc. |
| `STUB` | Placeholder — the interface exists but does no real work. Must not appear in the REAL layer. | legacy code (audited) |
| `TOKEN_VAZIO` | Explicit sentinel for a gap we choose to acknowledge instead of silently returning a default. Any REAL contract that finds this in a field it requires must fail-closed. | provenance defaults when `-D` isn't set at compile time |

---

## Comparison verdicts (used in `arch_capability/1.0.0`)

| Verdict | Meaning |
|---|---|
| `MATCH` | Observed value equals nominal exactly. |
| `OBSERVED_SUPERSET` | Observed is a strict superset of nominal (e.g. host has AVX512 while nominal lists only SSE2+AVX2). Not an error — observed wins. |
| `OBSERVED_MISMATCH` | Observed differs from nominal in a way that is not a superset. Surfaced as a NOTE; treated as authoritative truth. |
| `authority: observed_supersedes_nominal` | Explicit declaration in every capability JSON — nominal is a hint, observed is truth. |

---

## Source strings (used in `arch_capability/1.0.0`.`sources`)

Each observed field records where its value came from:

| Value | Meaning |
|---|---|
| `sysconf(_SC_PAGE_SIZE)` | libc `sysconf(2)` — POSIX standard |
| `sysconf(_SC_LEVEL1_DCACHE_LINESIZE)` | glibc extension — Linux + some others |
| `/sys/devices/.../coherency_line_size` | Linux sysfs — most authoritative |
| `/proc/cpuinfo` | Linux procfs — SIMD flag names |
| `uname(2)` | POSIX — always available |
| `PROBE_UNAVAILABLE_ON_THIS_OS` | The source expected to work was not available. Consumer decides how to treat this. |

---

## Governance stages (used in `real_governance.sh` log lines)

| Stage | Fail-closed condition |
|---|---|
| 1 | producer runs; exit 0 |
| 2 | strict Python JSON validation passes (duplicate/scope-aware) |
| 3 | C `contract-validate` accepts the JSON |
| 4 | no `TOKEN_VAZIO` substring in the output JSON |
| 5 | baseline passes strict validation and TOKEN_VAZIO scan |
| 6 | all regression fields present in both current and baseline |
| 7 | nodes ≥ baseline, edges ≥ baseline, phi ≥ baseline − 0.001, cycles = baseline, unresolved drift ≤ 5 |
| 8 | current receipt signature verifies |
| 9 | current receipt appends to ledger; full ledger chain verifies |
| 10 | runtime probe emits `arch_capability/1.0.0` + receipt; receipt appends to ledger; chain still verifies |

---

## Contract identifiers (semver)

| Identifier | Owned by | Purpose |
|---|---|---|
| `pkg_metrics/1.0.0` | `metrics-producer`, `real_contract.c`, `validate_pkg_metrics_json.py` | Real DAG measurements |
| `receipt/1.0.0` | `real_receipt.c`, `receipt-validate` | Signed operation receipt |
| `ledger_entry/1.0.0` | `real_ledger.c`, `receipt-ledger` | Chain-of-custody entry |
| `arch_capability/1.0.0` | `arch-probe`, `receipt-validate` | Runtime capability observation |
| `real_arch_matrix/1.0.0` | `arch-detect --json` | Nominal architecture catalog |
| `pkg_inventory_v1` | `pkg_scanner` | Filesystem inventory dump |
| `pkg_parser_v1` | `pkg_parser` | Single build.sh parse dump |
| `pkg_dag_v1` | `pkg_dag` | DAG summary dump |

Contract versions are semver strings. Existing enum values must never
be renumbered. Consumers MUST ignore unknown fields. Removing a field
requires a major version bump.

---

## Sentinel functions & macros

| Symbol | File | Meaning |
|---|---|---|
| `REAL_UNREACHABLE()` | `real_attrs.h` | `__builtin_unreachable()` — code path cannot execute |
| `REAL_LIKELY(x)` / `REAL_UNLIKELY(x)` | `real_attrs.h` | `__builtin_expect(!!(x), 1|0)` |
| `REAL_HOT` / `REAL_COLD` | `real_attrs.h` | places function in `.text.hot` / `.text.unlikely` section |
| `REAL_NONNULL(...)` | `real_attrs.h` | callers get `-Wnonnull-compare` on dead null-checks |
| `REAL_WARN_UNUSED` | `real_attrs.h` | forces caller to consume return value |
| `REAL_ARCH_BUILD` | `real_arch.h` | compile-time architecture identity from compiler predefines |
| `REAL_ARCH_UNKNOWN` | `real_arch.h` | sentinel: identity not recognized (never a claim) |
| `REAL_SYSCALL_UNKNOWN` | `real_arch.h` | sentinel: syscall convention not classified |
| `PROBE_UNAVAILABLE_ON_THIS_OS` | `arch_probe_cli.c` | runtime source not available; observation intentionally absent |

---

See also: [REAL.md](REAL.md) for full architecture, contracts, empirical
measurements, and boundaries.
