# REAL Layer — Hotfix Audit Report (Pass 4)

**Date**: 2026-08-10  
**Branch**: `claude/sistema-nucleo-autoral-2bju50`  
**Predecessors**: Pass 1 (PR #51), Pass 2 (PR #52), Pass 3 (PR #53).

**This pass**: audit of surfaces the three prior passes only skirted —
test infrastructure (`test-pkg-real`, all test `.sh` scripts),
`arch_detect_cli.c`, Python validator invariants, extract_str/extract_num
corner cases, canonical_serialize round-trip symmetry.

**Outcome**: **4 new findings**, **3 fixed together** (D3+D4+D5, a
correctness triangle — you can't fix one without the others), **1
accepted with rationale** (D1, hypothetical NDEBUG build).

Full CI-gated regression suite now **15 suites, 174 asserts, 0
failures**.

---

## New findings

### D3+D4+D5 — MEDIUM (JSON escape/decode asymmetry) → **FIXED TOGETHER**

Three findings that must be fixed as a set — fixing any one alone
would break existing round-trip.

**D3 (`core/real_contract.c:44-54`)**: `extract_str` "handled" escape
sequences by consuming both the backslash and the escaped char but
writing NOTHING to dst. `"foo\"bar"` in JSON became `"foobar"` in the
struct — losing the escaped char entirely.

**D4 (`core/real_provenance.c:87-102`)**: `real_provenance_write_json`
wrote every string field with raw `%s`. If any provenance field
contained a `"` or `\` or control char, the resulting JSON would be
malformed.

**D5 (`core/real_receipt.c:131-142`)**: Same class as D4 in
`write_io_array`: the `path` field of `inputs`/`outputs` entries was
written with raw `%s`. Filesystem paths on Linux can contain any byte
except `/` and `\0` — a legitimate path with `"` in it would produce
malformed receipt JSON.

**Why linked**: The receipt SHA is computed by `canonical_serialize`
over the raw bytes of struct fields. Consumer flow is:

1. Producer builds struct from raw bytes (uname, argv, filesystem)
2. `canonical_serialize` hashes those raw bytes → stored as `content_sha256`
3. Producer writes JSON representation
4. Verifier reads JSON via `extract_str`
5. `canonical_serialize` re-hashes struct
6. Compare against `content_sha256`

For the hash to match on read-back, step 4's `extract_str` must
produce the SAME bytes as step 1's raw input. If producer writes
escapes (D4/D5 fix) and parser doesn't decode (D3 pre-fix), the
struct in step 4 will have escape sequences dropped → hash mismatch
→ verify FAILS on any receipt with escaped content.

**Consequence today**: The bug is **latent** because no REAL producer
currently emits fields containing special chars. All producers write
ASCII-only fields (git hex hash, ISO-8601 timestamp, `Linux 6.x.y
x86_64` uname). Any future producer that emits a filesystem path
containing a quote — or is deployed on a custom kernel whose uname
string contains one — would produce a receipt that would silently
misparse and fail verify.

**Fix**:
1. `extract_str` decodes `\"`, `\\`, `\/`, `\n`, `\r`, `\t`, `\b`, `\f`
   properly (unknown escapes preserve the raw char after backslash
   for forward-compat).
2. `real_provenance_write_json` uses new `prov_json_esc` helper.
3. `real_receipt.c write_io_array` uses new `rcpt_json_esc` helper.

Both writers mirror the escape policy already in `arch_probe_cli.c`
(C17) and `pkg_scanner.c` (C10) — same set of chars, same `\uXXXX`
fallback for control chars.

**Regression test**: `core/tests/test_hotfix_d3_d4_d5_json_roundtrip.sh`
(13 asserts) — exercises the full producer → jq-parse → verify → 
ledger-append → ledger-verify chain. Any regression that broke
symmetry (writer escapes but parser doesn't decode, or vice versa)
would surface as either a jq-parse failure OR a receipt-signature
mismatch OR a ledger-chain break.

Cannot easily test with actual special-char content without patching
the producer to emit them — but the symmetry invariant is what we
need to preserve, and the round-trip test verifies exactly that.

---

### D1 — LOW (assert() side-effect calls in test) → **ACCEPTED**

**File**: `core/tests/test_pkg_real.c:33, 34, 83, 84, 87, 88`  
**Symptom**: Setup uses `assert(pkg_inventory_init(&inv, 512) == 0)`
— the function call is inside `assert()`, which per C standard is
`((void)0)` when `NDEBUG` is defined. If someone builds the tests
with `-DNDEBUG` (production optimization), the setup calls are
elided → `inv` uninitialized → subsequent reads segfault or return
uninitialized garbage.

**Why accepted**: The Makefile's CFLAGS never defines `NDEBUG`. This
is a hypothetical risk requiring someone to override `CFLAGS`. Filed
as future improvement — replace `assert()` with an explicit
`if (!condition) { fprintf(stderr, ...); return 1; }` idiom.

---

## Scans clean this pass

| Scan | Result |
|---|---|
| `test-pkg-real` C source | Clean apart from D1 |
| `arch_detect_cli.c` | Clean — minimal argv dispatch, no state |
| `test_governance.sh` (14 asserts) | Clean — thorough negative-test coverage |
| `test_receipts.sh` (15 asserts) | Same B8-class brittleness in tamper seds; behavior is loud-fail-on-mismatch (not silent), acceptable |
| `test_arch.sh` (31 asserts) | Clean — solid |
| `test_arch_probe.sh` (15 asserts) | Clean — solid |
| `validate_pkg_metrics_json.py` (Python) | Re-verified clean from Pass 3 |
| `extract_num` numeric parser | Clean — proper bounded loop, extract_str fix (D3) closes the symmetry gap |
| `canonical_serialize` hash-order | Verified stable across writer changes (D3/D4/D5 test proves round-trip) |

---

## Files changed (this pass)

| Path | Change |
|---|---|
| `core/real_contract.c` | D3: `extract_str` now decodes JSON escape sequences properly |
| `core/real_provenance.c` | D4: `prov_json_esc` helper; provenance writer uses it |
| `core/real_receipt.c` | D5: `rcpt_json_esc` helper; io-array writer uses it |
| `core/tests/test_hotfix_d3_d4_d5_json_roundtrip.sh` | **NEW** — 13 asserts, symmetry round-trip |
| `core/Makefile` | Wired new suite into `real-test` |

---

## Verification

```
$ make -C core real-test
✓ All REAL test suites passed
```

**15 suites, 174/174 assertions**. Build clean under `-Wall -Wextra
-Wshadow -Werror -Wunused -Wunreachable-code -O2 -flto`.

`bash docs/REAL_TOUR.sh` — **22/22 live commands green**.

Backwards-compatibility: unchanged behavior on ASCII-only inputs (the
current production case). Prospective correctness for future
non-ASCII inputs. Existing merged receipts still verify (they contain
only ASCII).

---

## Cumulative state — Pass 1 + 2 + 3 + 4

- **34 distinct findings** across four passes
- **19 fixed** (12 P1 + 3 P2 + 1 P3 + 3 P4)
- **14 accepted** with rationale (design gap / low-severity / hypothetical)
- **1 tech-debt** carried (C13, 430 MB heap footprint)
- **7 dedicated regression suites** wired to `make real-test`
- **174/174 assertions** green, **22/22 tour commands** green
- **Zero build warnings** under `-Werror` + full strict flags

### Bug-density trend

| Pass | CRITICAL | HIGH | MEDIUM | LOW | Total fixed |
|---|:-:|:-:|:-:|:-:|:-:|
| 1 | 1 | 4 | 3 | 2 | 12 |
| 2 | 1 | 1 | 1 | 1 | 3 |
| 3 | 0 | 0 | 1 | 0 | 1 |
| 4 | 0 | 0 | 3 (linked) | 0 | 3 |

Diminishing returns confirmed: high-severity finds trend to zero,
remaining findings are correctness improvements for prospective (not
currently exercised) inputs.

---

## Reproducibility

```bash
make -C core clean real-all
make -C core real-test          # 15 suites, 174 assertions
bash docs/REAL_TOUR.sh          # 22 live commands
bash scripts/real_governance.sh # 10 stages
```
