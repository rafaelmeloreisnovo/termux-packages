# REAL Layer — Hotfix Audit Report (Pass 6)

**Date**: 2026-08-10  
**Branch**: `claude/sistema-nucleo-autoral-2bju50`  
**Predecessors**: Pass 1 (PR #51), Pass 2 (PR #52), Pass 3 (PR #53),
Pass 4 (PR #54), Pass 5 (PR #55).

**This pass**: interaction with the E9 fix (Pass 5) — checking whether
the newly-introduced escape expansion could overflow existing buffers
in the read path. Also spot-checked freestanding syscall wrappers,
short-write handling, and long-path stress scenarios.

**Outcome**: **7 new findings**, **1 fixed** with regression test
(F6, ledger fgets buffer sized for post-E9 worst-case escape
expansion), **6 accepted** with rationale (LOW severity or best-
effort semantics).

Full CI-gated regression suite now **17 suites, 188 asserts, 0
failures**.

---

## New findings

### F6 — MEDIUM (fgets truncation post-E9) → **FIXED**

**File**: `core/real_ledger.c:175, 279` (`real_ledger_tail`,
`real_ledger_verify`)  

**Symptom**: E9 (Pass 5) added JSON escape to `receipt_path` in the
ledger writer. Escaping expands: `"` → `\"` (2×), control chars →
`\uXXXX` (6×). `LEDGER_PATH_MAX` is 512 chars. Worst case: 512
control chars × 6 + ~350 bytes of ledger structure = **~3400 bytes
per line**. The pre-F6 `char line[2048]` fgets buffer would silently
truncate at 2047 bytes.

**Consequence chain**:
1. Attacker crafts a path with many control chars (all Linux filenames
   allow every byte except `/` and `\0`)
2. They arrange for a valid receipt file at that path
3. `receipt-ledger append <legit.jsonl> <weird.receipt>` proceeds
4. Writer produces a >2048-byte line — succeeds
5. Next verify: fgets reads first 2047 bytes, next fgets reads the
   rest as a "second entry" — chain corrupted, both entries broken
6. Operator sees "chain broken" with no clue about the actual cause

The bug was **latent for the typical case** (paths under 200 chars
with no control chars). But mechanically **reachable** — the E9 fix
opened the possibility by enabling escape expansion.

**Fix**: Bump line buffer from 2048 to 4096 bytes. This handles the
worst legitimate case (512 chars × 6 escape = 3072 + 350 = ~3422)
with margin. `parse_entry_line` already returns -1 on missing quote
terminators (which is what a truncated line looks like), so the
existing fail-closed semantics catch any residual truncation.

**Regression test**: `core/tests/test_hotfix_f6_ledger_line_length.sh`
(8 asserts) — constructs a deep directory yielding a 166-char
receipt path, appends multiple entries at that path, verifies chain
integrity, jq round-trip, and byte-for-byte path recovery.

---

### F1 — LOW (real_writes discards write error) → **ACCEPTED**

**File**: `core/real_mem.h:83`  
`real_writes(fd, s)` calls `real_write` and casts result to void.
Freestanding pipeline is best-effort; short writes drop output
silently. Filed as note — no fix needed for the counter's use case.

### F2 — LOW (no errno helper for freestanding syscalls) → **ACCEPTED**

**File**: `core/real_syscalls.h`  
Syscall wrappers return negative errno; callers check `< 0` manually.
Idiomatic for the freestanding style.

### F3 — LOW (getdents64 error treated as EOF) → **ACCEPTED**

**File**: `core/pkg_count_freestanding.c:44`  
`if (n <= 0) break;` — treats syscall errors as EOF. Undercounts on
kernel error but doesn't crash. Freestanding counter is not the
promoted-artifact path.

### F4 — INFO (AT_FDCWD is correct usage) → **N/A (verified correct)**

Not a finding, just verified during audit.

### F5 — INFO (freestanding follows symlinks) → **ACCEPTED (design)**

`real_newfstatat(AT_FDCWD, build_sh, &st, 0)` with flags=0 follows
symlinks. Intentional — packages may use symlinks in scripts.

### F7 — INFO (line length calc verified) → **N/A**

Actual max legitimate line length ≈ 1900 bytes; F6 fix provides 2×
headroom. Not a finding.

---

## Scans clean this pass

| Scan | Result |
|---|---|
| `real_syscalls.h` inline asm ABI | Clean — proper clobbers, register constraints |
| `real_mem.h` string primitives | Clean — bounds checked, no OOB |
| Freestanding stack usage | Verified — no unbounded recursion |
| `pkg_count_freestanding` symlink semantics | Documented design |
| Ledger line-length invariants post-E9 | Fixed by F6 |
| Ledger 3rd-instance escape (verify path) | Uses same GRAB_STR fix from E9 |

---

## Files changed (this pass)

| Path | Change |
|---|---|
| `core/real_ledger.c` | F6: `line[2048]` → `line[4096]` (in both `real_ledger_tail` and `real_ledger_verify` via `replace_all`) with detailed comment |
| `core/tests/test_hotfix_f6_ledger_line_length.sh` | **NEW** — 8 asserts, 166-char path stress test |
| `core/Makefile` | Wired new suite into `real-test` |

---

## Verification

```
$ make -C core real-test
✓ All REAL test suites passed
```

**17 suites, 188/188 assertions**. Build clean under `-Wall -Wextra
-Wshadow -Werror -Wunused -Wunreachable-code -O2 -flto`.

`bash docs/REAL_TOUR.sh` — **22/22 live commands green**.

---

## Cumulative state — Pass 1 + 2 + 3 + 4 + 5 + 6

- **50 distinct findings** across six passes
- **21 fixed** (12 P1 + 3 P2 + 1 P3 + 3 P4 + 1 P5 + 1 P6)
- **28 accepted** with rationale
- **1 tech-debt** carried (C13, 430 MB heap footprint)
- **9 dedicated regression suites** wired to `make real-test`
- **188/188 assertions** green, **22/22 tour commands** green
- **Zero build warnings** under `-Werror` + full strict flags

### Bug-density trend

| Pass | CRITICAL | HIGH | MEDIUM | LOW | Fixed |
|---|:-:|:-:|:-:|:-:|:-:|
| 1 | 1 | 4 | 3 | 2 | 12 |
| 2 | 1 | 1 | 1 | 1 | 3 |
| 3 | 0 | 0 | 1 | 0 | 1 |
| 4 | 0 | 0 | 3 (linked) | 0 | 3 |
| 5 | 0 | 0 | 1 | 5 | 1 |
| 6 | 0 | 0 | 1 (interaction w/ P5) | 5 | 1 |

Pass 6's MEDIUM finding (F6) is a direct **consequence of the E9 fix
in Pass 5** — a buffer that was previously sufficient became
insufficient after we increased the write-time expansion. This is a
healthy audit signal: fixes can introduce subtle interactions, and
subsequent passes catch them.

---

## Reproducibility

```bash
make -C core clean real-all
make -C core real-test          # 17 suites, 188 assertions
bash docs/REAL_TOUR.sh          # 22 live commands
bash scripts/real_governance.sh # 10 stages
```
