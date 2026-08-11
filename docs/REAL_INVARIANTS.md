# REAL Layer — Structural Invariants (Sustentation Contract)

**Purpose**: this document names, in ONE place, every invariant the
REAL layer maintains. An invariant here is a **structural condition
that must hold before, during, and after every operation** — losing
one invalidates the layer's claim of authenticity.

Each invariant is:
1. **Named** (short human-readable identifier).
2. **Located** (file + function/module that enforces it).
3. **Tested** (regression test that would fail if it broke — cited
   by suite name).
4. **Failure mode** (what happens fail-closed when the invariant is
   violated by input, hardware, or attacker).

If an invariant lists no failing test, that is a gap and MUST be
filled before the invariant is trusted.

---

## Module: `pkg_scanner` — filesystem inventory

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| S1 | Coverage of the four known repository roots is EXPLICIT — an absent root increments `roots_absent`, never disappears silently | `pkg_inventory_scan_all` | `test-pkg-real:test_inventory_real` | `is_complete()` returns 0 → producer emits `INCOMPLETE`, contract rejects |
| S2 | No path overflow, I/O error, allocation failure, or subpackage scan failure may be silent | `pkg_inventory_scan_repo`, `scan_subpackages` | `test-pkg-real:test_inventory_real` (checks all-zero error counters) | any counter ≠ 0 → is_complete = false → contract rejects |
| S3 | JSON output of scanner fields (name, parent, path) is bytewise-valid — special chars are escaped | `pkg_json_esc` (C10 fix) | `test_hotfix_c17_json_escape.sh` | jq parse fails downstream → operator sees explicit malformation |

---

## Module: `pkg_parser` — build.sh reader

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| P1 | No shell execution — parser is a static reader; malicious build.sh cannot execute code | `pkg_parser_parse_file` (line-by-line lex only, no `system()`) | Explicit design; audited | none — arbitrary build.sh can only inflate counters |
| P2 | Function bodies are skipped — `TERMUX_PKG_X` assignments inside `foo() { ... }` do not leak | `line_starts_function` + brace depth tracking | `test-pkg-real:test_parser_real` | leaked assignment would show up as a spurious `var` — contract does not consume vars |
| P3 | Overlong dependency fields (≥128 chars) are NEVER silently truncated — DAG builder returns -1 | `pkg_dag.c split_deps` (`dependency_field_overflows` counter) | metrics-producer BLOCKED path 1 (line 105) | governance gate fails at step 3 |
| P4 | Parse-failures are counted, never hidden | `pkg_dag.c pkg_dag_build` (`parse_failures` counter) | metrics-producer BLOCKED path (line 113) | governance gate blocks; contract validator rejects |

---

## Module: `pkg_dag` — dependency graph

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| D1 | Cycles are detected via Tarjan SCC + self-loop test | `record_cyclic_sccs`, `has_self_loop` | `test-pkg-real:test_dag_real` | acyclicity = 0 → coherence_phi = 0 → contract validator rejects |
| D2 | Topological order covers every acyclic node | `pkg_dag_topo_sort` (`topo_count == inv.count` for acyclic graph) | contract validator: `cycle_count=0 → topo_ordered==node_count` | contract validator rejects |
| D3 | `edge_count == depends_edges + build_dep_edges` (partition invariant) | `real_contract.c:209` cross-field check | `test_governance.sh` invariant tests | contract violation reported |
| D4 | `graph_completeness == 1 - unresolved/edges` (bounded 0..1) | `real_contract.c:242` cross-field check | `test_governance.sh` invariant tests | contract violation reported |
| D5 | `coherence_phi == graph_completeness × graph_acyclicity` | `real_contract.c:223` cross-field check | `test_governance.sh` (n3 test) | contract violation reported |

---

## Module: `real_provenance` — build/runtime metadata

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| PR1 | Every provenance field is non-empty AND does not contain `TOKEN_VAZIO` | `real_contract.c require_str` | `test_governance.sh` (test 4 + 7) | contract violation; governance rejects |
| PR2 | Truncation of `toolchain_id` or `host_uname` is surfaced as a `TOKEN_VAZIO_*_truncated` sentinel (not silently truncated) | `real_provenance.c` (snprintf return-check) | `test_receipts.sh` (provenance non-empty checks) | contract validator rejects the sentinel |
| PR3 | Provenance JSON fields survive byte-symmetric round-trip through the JSON escape/decode pair | `prov_json_esc` (D4) + `extract_str` (D3) | `test_hotfix_d3_d4_d5_json_roundtrip.sh` | verify FAILS on mismatch |

---

## Module: `real_receipt` — signed operation record

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| R1 | `content_sha256` is SHA256 over the canonical serialization of every other receipt field | `real_receipt_seal` → `canonical_serialize` | `test_receipts.sh` tamper tests | verify_file rejects mismatch |
| R2 | Parsed provenance strings are INSTANCE-owned (no static aliasing between two parsed receipts) | `real_receipt.h` (`_prov_*` backing fields; B1 fix) | `test_hotfix_b1_receipt_aliasing.sh` | 3rd verify of a receipt would silently return truncated struct |
| R3 | Numeric fields are rejected at parse time when unparseable (`strtoull`/`strtol` with `errno`+endptr) | B3/B4 fixes | `test_hotfix_b3_b4_strto_guards.sh` | verify_file returns -1 |
| R4 | JSON write is atomic — either the target file has a complete valid receipt or it does not exist | `real_receipt_write` (tmp + rename; B11/B12) | Implicit: partial-write recovery preserved by tmp-then-rename | Consumers never see a partially-written receipt |
| R5 | Writer refuses to follow a symlink at the tmp path (defence-in-depth) | `O_NOFOLLOW | O_EXCL` (C40) | `test_hotfix_c40_symlink_hardening.sh` | open fails → -1 returned |
| R6 | Producer exit code reflects receipt write outcome (0 = full success including receipt; 3 = JSON OK but receipt failed) | metrics-producer + arch-probe (G6) | `test_hotfix_g6_producer_exit_code.sh` | governance sees exit ≠ 0 → BLOCK |

---

## Module: `real_ledger` — chain-of-custody

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| L1 | Each entry's `entry_sha256` is SHA256 over its canonical form | `seal_entry` | `test_ledger.sh` tamper tests | verify BROKEN |
| L2 | Each entry's `prev_tail_sha256` equals the previous entry's `entry_sha256` | `real_ledger_verify` | `test_ledger.sh` | verify BROKEN at first mismatch |
| L3 | Sequence numbers are strictly sequential from 0 | `real_ledger_verify` | `test_ledger.sh` seq test | verify BROKEN |
| L4 | Concurrent appenders are serialized — no two entries can race the same next_seq | `flock(LOCK_EX)` (B6) | `test_hotfix_b6_ledger_concurrent.sh` (10 concurrent writers) | 2nd racer waits for lock |
| L5 | Failed append leaves the ledger BYTE-IDENTICAL to its pre-append state (transactional isolation) | `lseek(SEEK_END)` + `ftruncate` rollback (C23) | `test_hotfix_c23_ledger_rollback.sh` | rollback fires; retry-safe |
| L6 | Append refuses to follow a symlink at the ledger path (defence-in-depth) | `O_NOFOLLOW` (C40) | `test_hotfix_c40_symlink_hardening.sh` | open fails → -1 |
| L7 | Referenced receipt file is still present AND its content_sha256 still matches the recorded receipt_sha256 | `real_ledger_verify` walks each entry | `test_ledger.sh` tests 7+10 (edit + move) | verify BROKEN |
| L8 | `receipt_path` round-trips byte-for-byte through the JSON escape/decode pair | `ledger_json_esc` (E9) + `GRAB_STR` decode (E9) | `test_hotfix_e9_ledger_path_escape.sh` | verify BROKEN on mismatch |
| L9 | Ledger lines never truncate — fgets buffer accommodates worst-case escape expansion | 4096-byte line buffer (F6) | `test_hotfix_f6_ledger_line_length.sh` | truncation → parse_entry_line rejects → verify BROKEN |

---

## Module: `real_contract` — strict validator

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| C1 | `schema` field must equal `pkg_metrics/1.0.0` exactly | `real_contract_validate_file` | `test_governance.sh` | contract violation |
| C2 | `status` field must equal `REAL` exactly | same | `test_governance.sh` (n1 negative test) | contract violation |
| C3 | No JSON substring `TOKEN_VAZIO` may appear in a promoted artifact | `require_str` + governance script `require_no_token_vazio` | `test_governance.sh` (n2 negative test) | contract + governance both block |
| C4 | Numeric fields survive `errno=0`-guarded `strtoul/strtoull/strtod` (no silent zero on garbage) | `require_u32/u64/double` (B2 fix for strtod) | `test_hotfix_b3_b4_strto_guards.sh` | validation reports "unparseable" |
| C5 | JSON escape sequences in string fields round-trip byte-symmetric | `extract_str` decode (D3) + `prov_json_esc` write (D4) | `test_hotfix_d3_d4_d5_json_roundtrip.sh` | canonical SHA mismatch → verify fails |

---

## Module: `real_arch` — nominal + observed identity

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| A1 | The nominal table has status `OBSERVED_LIMITED` and `claim_allowed=false` — property fields are NOMINAL, not runtime evidence | `real_arch_write_json` header fields | `test_arch.sh` (scope test) | any consumer reading this table for runtime claim violates its own contract |
| A2 | Compile-time architecture identity is derived from compiler predefines (no runtime probing) | `REAL_ARCH_BUILD` macros in `real_arch.h` | `test_arch.sh` (compile identity test) | fail-closed via mismatch report |
| A3 | Runtime architecture identity is derived from `uname(2)` — a real syscall, not a claim | `real_arch_detect_runtime` | `test_arch.sh` (runtime identity test) | UNKNOWN sentinel returned on mismatch |
| A4 | Runtime capability probe (page_size, cache_line, SIMD) records SOURCE per field — no silent zero | `arch_probe_cli.c` (source_src variables) | `test_arch_probe.sh` (sources checks) | `PROBE_UNAVAILABLE_ON_THIS_OS` sentinel |
| A5 | Observed data supersedes nominal — declaring `authority: observed_supersedes_nominal` in every capability JSON | `arch_probe_cli.c` line 201 | `test_arch_probe.sh` (authority test) | consumers ignoring this violate their own contract |

---

## Module: `real_governance.sh` — 10-stage gate

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| G1 | ALL 10 stages must pass — any single BLOCKED stage aborts governance with exit 1 | `fail()` function terminates script | `test_governance.sh` (test 10 end-to-end) | script exits non-zero |
| G2 | Each governance run uses a unique per-run `_STAMP` (never collides even on same-second same-pid) | `mktemp -u` (B10) | governance runs cannot overwrite each other | receipt/ledger references stay honest |
| G3 | Regression fields must be present in BOTH current and baseline before comparison | `require_json_field` loop | `test_governance.sh` (test 6+11+12+13+14) | BLOCK if either missing |
| G4 | Regression thresholds are enforced numerically — nodes/edges never allowed to decrease | direct integer comparison | `test_governance.sh` test 7 | script exits non-zero |
| G5 | Receipt signature must verify BEFORE ledger append (a bad receipt cannot enter the chain) | `real_ledger_append` calls `real_receipt_verify_file` first | `test_ledger.sh` tamper tests | append returns -1 |
| G6 | Runtime capability probe (step 10) must produce BOTH JSON AND signed receipt | explicit `[ ! -f "$CAP_JSON.receipt" ]` check | `test_arch_probe.sh` | script exits non-zero |

---

## Meta-invariants (audit methodology itself)

| # | Invariant | Enforced by | Verified by | Failure mode |
|---|---|---|---|---|
| M1 | Every hotfix bug (B-F class) has a dedicated regression test wired into `make real-test` | `Makefile real-test` target | 10 hotfix suites live | test suite count decrease alerts |
| M2 | Executable documentation `docs/REAL_TOUR.sh` runs every REAL binary + every test suite each generation | `docs/REAL_TOUR.sh` (self-updating) | `bash docs/REAL_TOUR.sh` (22 commands) | tour exit non-zero |
| M3 | Every fix comment cites the finding ID and previous-pass link (traceability) | Code review | Grep for `B[0-9]/C[0-9]/D[0-9]/E[0-9]/F[0-9]/G[0-9]` comments | reviewer can trace fix ↔ audit doc |
| M4 | Every audit pass documents its findings in `docs/REAL_HOTFIX_AUDIT_PASS<N>.md` — no accepted finding lacks a rationale | Manual per-pass discipline | Grep for accepted-with-rationale sections | operator can revisit accepted risks |
| M5 | Cumulative bug-density trend must move monotonically down (or explain outliers) | This document + per-pass reports | Comparison table in each PASS report | outlier requires explanation |

---

## What this document is NOT

- It is NOT a proof of security in the cryptographic-signature sense
  (no private key involved).
- It is NOT a proof of buildability, product readiness, or Android
  runtime.
- It is NOT a completeness proof for the upstream 3000+ package
  build scripts — those are outside the REAL layer scope.

## What this document IS

- A structural map: each invariant is named, located, tested, and
  its failure mode is known.
- A trust surface: consumers of the REAL layer can point at this
  file and say "I depend on these N invariants; here is how they
  are enforced".
- An audit ledger: gaps in this document are gaps in the layer.
  Filling them is real work, not documentation work.

## When to update this document

- When adding a NEW invariant (new module, new field): add a row.
- When fixing a bug: update the "Verified by" cell if the fix
  added a new regression test.
- When accepting a risk: NOT here. Accepted risks live in the
  per-pass audit reports.
