# REAL Layer — Architecture & Contracts

**Status:** REAL for the modules listed here. Scope-limited to what is
measured; boundaries are explicit at the end.

**Purpose:** provide an authoritative, provenance-carrying, tamper-evident
evidence chain for every measurement the build system emits. The REAL
layer is the answer to "how do you know?" for every number in every
artifact.

---

## 1. Design Principles

Six principles bind every REAL module together:

| Principle | Enforcement |
|---|---|
| **Real sources only** | No hardcoded metrics, no `rand()`, no simulated timings; every field is derived from a real syscall, file read, or hash. |
| **Fail-closed** | Absent inputs / invalid signatures / broken invariants ⇒ exit non-zero. Never silently succeed. |
| **Contract-first** | Every JSON artifact declares a `schema` (semver) and a `status`. Consumers reject anything else. |
| **Provenance-carrying** | Every artifact embeds git commit, build timestamp, toolchain, host, arch. |
| **Tamper-evident** | Every receipt is SHA256-sealed; every ledger entry chains to the previous. Any edit breaks verification. |
| **Observed supersedes nominal** | Static catalogs are marked `OBSERVED_LIMITED`. Runtime probes emit `status: OBSERVED` with `authority: observed_supersedes_nominal`. |

---

## 2. Module Index

| Module | Status | Contract | Purpose |
|---|---|---|---|
| `core/pkg_scanner.{h,c}` | REAL | inventory | Discover every real `build.sh` + `*.subpackage.sh` |
| `core/pkg_parser.{h,c}` | REAL | parsed vars | Extract `TERMUX_PKG_*` fields with no shell execution |
| `core/pkg_dag.{h,c}` | REAL | dependency graph | Build real DAG + cycles + topo sort |
| `core/real_provenance.{h,c}` | REAL | prov v1 | Capture git/toolchain/host/time at compile+run |
| `core/real_arch.{h,c}` | OBSERVED_LIMITED | 15-arch nominal | Static identity catalog + nominal reference matrix |
| `core/real_contract.{h,c}` | REAL | `pkg_metrics/1.0.0` | Schema validator + cross-field invariants |
| `core/real_sha256.{h,c}` | REAL | FIPS 180-4 | Self-contained SHA-256 (no OpenSSL) |
| `core/real_receipt.{h,c}` | REAL | `receipt/1.0.0` | Signed receipt lifecycle: begin → seal → verify |
| `core/real_ledger.{h,c}` | REAL | `ledger_entry/1.0.0` | Append-only hash-chained ledger |
| `core/real_attrs.h` | REAL | header | Compiler → linker directives (HOT, COLD, NONNULL, LIKELY, ...) |
| `core/real_syscalls.h` | REAL | header | Linux x86_64 syscalls via inline asm (freestanding) |
| `core/real_mem.h` | REAL | header | Freestanding mem/str primitives (no libc) |
| `core/metrics_producer.c` | REAL | `pkg_metrics/1.0.0` | Emits real graph metrics + receipt |
| `core/arch_probe_cli.c` | OBSERVED | `arch_capability/1.0.0` | Runtime probe: syscall/`/proc`/`/sys` |
| `core/pkg_count_freestanding.c` | REAL | text output | No libc, no CRT, direct syscalls |
| `core/contract_validate_cli.c` | REAL | CLI | `contract-validate <json>` |
| `core/receipt_validate_cli.c` | REAL | CLI | `receipt-validate <receipt>` |
| `core/receipt_ledger_cli.c` | REAL | CLI | `receipt-ledger {append,verify,tail}` |
| `core/arch_detect_cli.c` | OBSERVED_LIMITED | CLI | `arch-detect [--json\|--compat A B]` |
| `scripts/real_governance.sh` | REAL | 10-stage gate | End-to-end fail-closed governance |
| `scripts/real_arch_runtime_probe.py` | OBSERVED | JSON | Companion Python probe (main-branch addition) |

---

## 3. Contracts (schemas)

### 3.1 `pkg_metrics/1.0.0` — Real DAG measurements

Emitted by `metrics-producer`. Consumed by `contract-validate`, the
strict Python validator, `health_check.sh`, and the governance gate.

**Required top-level fields (fail-closed if absent):**

| Field | Type | Bound / Invariant |
|---|---|---|
| `schema` | string | must equal `"pkg_metrics/1.0.0"` |
| `status` | string | must equal `"REAL"` |
| `generated_unix_ms` | uint64 | wall-clock time of emission |
| `repo_base` | string | filesystem path scanned |
| `provenance` | object | see §3.5 |
| `arch` | object | `{compile_time, runtime, ...}` from `real_arch` |
| `inventory_coverage` | object | roots + error counters (all must be zero for promotion) |
| `parse_failures` | uint32 | zero for promotion |
| `alternative_dep_fields` | uint32 | count of `a|b` deps encountered |
| `dependency_field_overflows` | uint32 | must be zero (truncation intolerable) |
| `node_count`, `edge_count`, `depends_edges`, `build_dep_edges`, `unresolved_count`, `cycle_count`, `max_depth`, `topo_ordered` | uint32 | numeric |
| `coherence_phi`, `graph_completeness`, `graph_acyclicity` | double | in `[0.0, 1.0]` |
| `avg_deps_per_pkg` | double | in `[0.0, 10000.0]` |
| `inventory_latency_us`, `dag_latency_us`, `total_latency_us` | uint64 | wall time per phase |

**Cross-field invariants (enforced by `real_contract.c`):**

```
edge_count            == depends_edges + build_dep_edges
coherence_phi         == graph_completeness × graph_acyclicity   (±0.0001)
graph_completeness    == 1 − unresolved_count / edge_count       (±0.0001)
cycle_count == 0      ⇒ topo_ordered == node_count
cycle_count  > 0      ⇒ graph_acyclicity < 1.0
node_count             > 0
```

### 3.2 `receipt/1.0.0` — Operation receipt

Emitted by any REAL binary that runs a discrete operation.

**Fields:**

```
schema, status="REAL", operation, content_sha256,
exit_code, duration_us, started_unix_ms, finished_unix_ms,
arch { compile_time, runtime },
provenance { … },
inputs  [ { path, sha256, size }, … ],
outputs [ { path, sha256, size }, … ]
```

**Signature:** `content_sha256` is `SHA256(canonical(all_fields_above))`.
`real_receipt_verify_file()` recomputes and rejects on mismatch.

### 3.3 `ledger_entry/1.0.0` — Chain-of-custody entry

Emitted by `receipt-ledger append`. JSONL, one entry per line.

**Fields:**

```
schema, seq, prev_tail_sha256, receipt_sha256, receipt_path,
appended_unix_ms, entry_sha256
```

**Chain invariant:** `entry[N].prev_tail_sha256 == entry[N-1].entry_sha256`.
Entry 0 has empty `prev_tail_sha256`. `entry_sha256` seals canonical
form of the fields above (excluding itself).

### 3.4 `arch_capability/1.0.0` — Runtime capability observation

Emitted by `arch-probe`. Status is `OBSERVED` (not `REAL`) to distinguish
observation from measurement.

**Fields:**

```
schema, status="OBSERVED", generated_unix_ms,
identity  { compile_time, runtime, uname_sysname, uname_release, uname_machine },
observed  { page_size, cache_line, simd[] },
sources   { page_size, cache_line, simd },   ← per-field provenance
nominal   { page_size, cache_line, simd[] }, ← from real_arch table
comparison{ page_size, cache_line, simd, authority="observed_supersedes_nominal" }
```

`sources[field]` is either the actual syscall/path used (e.g.
`"sysconf(_SC_PAGE_SIZE)"`, `"/proc/cpuinfo"`) or the sentinel
`"PROBE_UNAVAILABLE_ON_THIS_OS"` on non-Linux hosts.

### 3.5 Provenance block (embedded in every REAL artifact)

```
provenance {
  schema_version:        "<contract that owns this file>",
  git_commit:            "<full 40-char SHA of tree that built the binary>",
  build_timestamp_utc:   "<ISO-8601 UTC>",
  cflags_fingerprint:    "<first 16 hex of SHA256(REAL_CFLAGS)>",
  toolchain_id:          "<gcc|clang> <__VERSION__>",
  producer_name:         "<basename(argv[0])>",
  run_timestamp_unix_ms: <CLOCK_REALTIME when the producer ran>,
  host_uname:            "<sysname release machine>"
}
```

Every string must be non-empty and free of the literal `TOKEN_VAZIO`.
Missing → contract violation → gate fails.

---

## 4. Fail-Closed Behaviors (catalog)

| Trigger | Detected by | Action |
|---|---|---|
| Missing `build.sh` in a package dir | `pkg_scanner` | counted as `TOKEN_VAZIO`, reported |
| Unresolved dependency name | `pkg_dag` | tracked in `unresolved_count`, flagged in report |
| Dependency field overflow | `pkg_dag` | must be 0; `metrics_producer` refuses to emit |
| Cycle detected | `pkg_dag` | `cycle_count > 0` → `graph_acyclicity < 1` → downstream gates flag |
| Provenance capture fails | `real_provenance_capture` | `metrics-producer` returns 2 |
| `metrics_current.json` missing | `health_check.sh` | exit 1 (CRITICAL, was WARNING) |
| JSON not `status="REAL"` | `contract-validate` | exit 1 |
| Any provenance string contains `TOKEN_VAZIO` | `real_contract` | exit 1 |
| `coherence_phi` out of `[0,1]` | `real_contract` | exit 1 |
| `edges != depends + build_deps` | `real_contract` | exit 1 |
| Cross-field math violates definition | `real_contract` | exit 1 |
| Receipt content edited | `real_receipt_verify_file` | SHA mismatch → exit 1 |
| Ledger entry deleted/edited/reordered | `real_ledger_verify` | chain break at exact position |
| Referenced receipt file removed | `real_ledger_verify` | fopen fail → chain break |
| Regression: nodes ↓, edges ↓, phi drops > 0.001, cycles ≠ baseline, unresolved drift > 5 | governance gate | exit 1 |

---

## 5. The 10-Stage Governance Gate

`scripts/real_governance.sh` — every stage is fail-closed; any failure
aborts the gate and prints the exact reason.

```
step 1/10: run producer                                     (metrics-producer)
step 2/10: strict current JSON validation                   (validate_pkg_metrics_json.py)
step 3/10: C contract validation (pkg_metrics/1.0.0)        (contract-validate)
step 4/10: scan current output for TOKEN_VAZIO              (grep + fail)
step 5/10: strict regression baseline validation            (validate_pkg_metrics_json.py)
step 6/10: regression field presence                        (jq -e)
step 7/10: regression gate vs baseline                      (jq + awk)
step 8/10: verify receipt signature                         (receipt-validate)
step 9/10: chain-of-custody ledger append + verify          (receipt-ledger)
step 10/10: runtime capability probe + receipt + ledger     (arch-probe + ledger)
```

Timestamped output paths (`$LEDGER_DIR/metrics-<STAMP>.json`) ensure
runs never overwrite each other and the ledger grows monotonically.

---

## 6. Empirical Measurements (on this repository)

Numbers verified against real filesystem + real syscalls:

| Measurement | Value | Source |
|---|---:|---|
| Package directories scanned | 2991 | `packages/` + `root-packages/` + `x11-packages/` + `disabled-packages/` |
| Subpackages discovered | 405 | `*.subpackage.sh` under each |
| **Total nodes** | **3396** | inventory count |
| DEPENDS edges | 12,679 | from `TERMUX_PKG_DEPENDS` |
| BUILD_DEPENDS edges | 1,777 | from `TERMUX_PKG_BUILD_DEPENDS` |
| **Total edges** | **14,456** | sum |
| **Cycles** | **0** | Kahn's algorithm |
| Unresolved deps | 55 | flagged as TOKEN_VAZIO |
| Max topological depth | 28 | real, not 42 (which was assumed) |
| `coherence_phi` | 0.996195 | `completeness × acyclicity` |

Independent freestanding verification:

| Binary | Text size | Total size | libc |
|---|---:|---:|---|
| `pkg-real` (libc, no LTO) | 29,810 B | 74,448 B | glibc |
| `metrics-producer` (REAL preset, LTO) | 16,327 B | 87,808 B | glibc |
| `pkg-count-freestanding` | **2,186 B** | **9,496 B** | **none** |

Freestanding produces identical counts using zero libc symbols and a
naked `_start`.

---

## 7. Test Evidence

| Suite | Assertions | Status |
|---|---:|---|
| `core/test-pkg-real` | 25 | pass |
| `core/tests/test_real_attrs.sh` | 5 | pass |
| `core/tests/test_freestanding.sh` | 7 | pass |
| `core/tests/test_governance.sh` | 14 | pass |
| `core/tests/test_arch.sh` | 31 | pass |
| `core/tests/test_receipts.sh` | 15 | pass |
| `core/tests/test_ledger.sh` | 15 | pass |
| `core/tests/test_arch_probe.sh` | 15 | pass |
| **REAL layer total** | **127** | **127/127 pass** |

`scripts/real_governance.sh` — 10/10 stages green.

---

## 8. Build System

The Makefile carries three build presets:

```
CFLAGS                     -Wall -Wextra -Wshadow -Werror -g -O0
REAL_CFLAGS   (LTO/GC)     + -Wunused -Wunreachable-code -O2 -g
                           + -fdata-sections -ffunction-sections
                           + -fvisibility=hidden -flto
REAL_LDFLAGS               -Wl,--gc-sections -flto

FREESTANDING_CFLAGS        -O2 -nostdlib -nostartfiles -ffreestanding
                           -static -fno-builtin -fno-stack-protector
                           -no-pie + sections + visibility
FREESTANDING_LDFLAGS       -nostdlib -nostartfiles -static -no-pie
                           -Wl,--gc-sections -Wl,--build-id=none
```

Every REAL binary is compiled with `REAL_PROV_DEFS`:

```
-DREAL_GIT_COMMIT="<git rev-parse HEAD>"
-DREAL_BUILD_TIMESTAMP="<date -u ISO-8601>"
-DREAL_CFLAGS_FP="<sha256(REAL_CFLAGS)[:16]>"
```

So the git tree and build environment are baked into the binary and
appear in every artifact it emits.

---

## 9. Mini-Block Attribute Directives

`real_attrs.h` provides per-function directives that the compiler
forwards to the linker as part of `-Wl,--gc-sections + -flto`:

| Macro | Effect |
|---|---|
| `REAL_HOT` / `REAL_COLD` | steers code into `.text.hot` / `.text.unlikely` |
| `REAL_PURE` / `REAL_CONST` | enables CSE + loop-invariant motion |
| `REAL_NONNULL(...)` | null-check elision + `-Wnonnull-compare` warns on dead checks |
| `REAL_WARN_UNUSED` | forces callers to consume return values |
| `REAL_HIDDEN` | per-symbol hidden; `--gc-sections` drops if unused |
| `REAL_LIKELY(x)` / `REAL_UNLIKELY(x)` | branch reorder |
| `REAL_UNREACHABLE()` | eliminates entire code path |
| `REAL_NORETURN` | no epilogue emitted; tail becomes bare `jmp` |
| `REAL_FLATTEN` | inlines all leaf calls |
| `REAL_ALWAYS_INLINE` / `REAL_NOINLINE` | inline decisions |
| `REAL_SECTION(name)` / `REAL_ALIGNED(n)` | placement + alignment |

The warnings ARE the friction reduction: a `-Wnonnull-compare` firing
means a defensive `if (!ptr)` is dead code — removing it (not silencing
it) shrinks the binary.

---

## 10. Boundaries (what the REAL layer does NOT prove)

The gate is scope-limited. Passing it validates the following:

**Proves:**
- `pkg_metrics/1.0.0` shape + math invariants
- Provenance (git/toolchain/host/time/arch) is captured non-empty
- Receipts sign every emission; ledger chains them
- Runtime capability probe records observed hardware
- No metric regression vs the tracked baseline

**Does NOT prove:**
- Package buildability against actual toolchain / SDK
- Android or device runtime behavior
- Cross-architecture portability (only nominal identity; execution
  requires a separate receipt per target)
- Cryptographic authenticity of the toolchain (`toolchain_id` is
  the compiler's self-report, not signed by any CA)
- End-to-end product readiness

Every REAL JSON declares `claim_allowed: false` for scope beyond what
its schema covers.

---

## 11. Where To Look

| To do this | Read this |
|---|---|
| Generate a real metrics JSON | `core/metrics_producer.c` |
| Add a new required field to the contract | `core/real_contract.h` + `.c` |
| Understand receipt canonical form | `core/real_receipt.c` `canonical_serialize()` |
| Understand ledger chain-hash | `core/real_ledger.c` `entry_canonical()` |
| Add a new architecture identifier | `core/real_arch.h` enum + `k_props[]` in `real_arch.c` |
| Add a new attribute directive | `core/real_attrs.h` |
| Add a new syscall to freestanding | `core/real_syscalls.h` |
| Run the full gate | `bash scripts/real_governance.sh` |
| Run one specific test suite | `bash core/tests/test_<name>.sh` |
| See the empirical DAG snapshot | `core/tests/fixtures/real_dag_baseline.json` |

---

## 12. Evolution Policy

- Contract versions are **semver strings** (e.g. `pkg_metrics/1.0.0`).
- Existing enum values (arches, syscall conventions) MUST NOT be renumbered.
- Fields MAY be added; consumers MUST ignore unknown fields.
- Removing a field requires a major version bump.
- The regression gate blocks: nodes ↓, edges ↓, phi drops > 0.001,
  cycles ≠ baseline, unresolved drift > 5.
- The ledger is append-only; entries are immutable once written.
- Timestamped output paths mean every run adds one entry; the chain
  grows monotonically (evolução sem retração).

---

_Generated by the REAL layer authors. Every claim in this document is
backed by a test in `core/tests/` or a measurement in
`core/tests/fixtures/real_dag_baseline.json`._
