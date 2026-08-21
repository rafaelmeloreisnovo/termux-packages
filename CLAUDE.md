# CLAUDE.md — Claude Code adapter for termux-packages

@AGENTS.md
@docs/00-INDEX.md
@core/STATUS.md

This file is a Claude Code adapter, not a second source of architectural truth.
The repository-wide contract is `AGENTS.md`; the detailed protocol is in `docs/`.

## Session start

Before editing:

1. Read `AGENTS.md` and `core/STATUS.md` for ground truth on TOKEN_VAZIO and cycle state.
2. Read `docs/00-INDEX.md` and subsystem docs for the changed path.
3. Inspect branch, HEAD and working tree:
   ```sh
   git branch --show-current
   git rev-parse HEAD
   git status --short
   ```
4. Identify which statements are IMPLEMENTED, PASS, FAIL, or TOKEN_VAZIO.
5. Run applicable baseline before modification (e.g., `make syntax` if available).

Do not merge without explicit human authorization.

## Project orientation

termux-packages combines:

- `core/` — Cycle 1.5 hardening: artifact validation, manifest lookup, directory organization
- `packages/` — 2057 active packages with interdependencies
- `tests/` — Gate-based validation (cycle 1, 2, 3+)
- `docs/` — Architecture, governance, gates, cycles
- `scripts/`, `.github/workflows/` — Build orchestration and CI

**Do not reduce the whole repository to one subsystem.** The package ecosystem depends on cycles and gates in strict order.

## Critical truth corrections

### Manifesto string pool bug

**Known issue (documented in STATUS.md):** Package name corruption in multi-arch manifest entries. Manifesto V2 (Cycle 2) will fix offsets and bounds checking.

**Do NOT:**
- Assume architecture lookup (e.g., `termux_find_package_by_arch`) is bijective
- Silence lookup failures with fallback without logging

**DO:**
- Preserve explicit NULL checks on manifest lookups
- Bind all architecture-specific builds to readelf proof (ELF Machine field)

### TOKEN_VAZIO vs. IMPLEMENTED

**Current state (as of core/STATUS.md):**

| Claim | State | Cycle |
|-------|-------|-------|
| Artifact validation | ✅ IMPLEMENTED_UNTESTED | 1.5 |
| TAR reproducibility | ✅ PROVEN_LOCAL | 1.5 |
| ELF architecture proof | ✅ IMPLEMENTED_UNTESTED | 1.5 |
| **SOURCE_FETCH** | ❌ TOKEN_VAZIO | 2 |
| **SOURCE_EXTRACT** | ❌ TOKEN_VAZIO | 2 |
| **PATCH_APPLY** | ❌ TOKEN_VAZIO | 2 |
| **MANIFEST_BINDING** (V2) | ❌ TOKEN_VAZIO | 2 |
| **DEP_GRAPH** | ❌ TOKEN_VAZIO | 2 |
| ARMV7_ELF (device) | ❌ TOKEN_VAZIO | 3 |
| AARCH64_ELF (device) | ❌ TOKEN_VAZIO | 3 |

**Do NOT:**
- Erase TOKEN_VAZIO markers without implementing the corresponding gate
- Promote IMPLEMENTED_UNTESTED to PASS without device evidence
- Mix cycle gates; Cycle 2 blockers must close before Cycle 3 starts

### Current-commit evidence

Do not promote source/ELF/APK existence into device-runtime proof.

```text
source (in packages/)
  != source download + SHA-256 verification (TV-01)
  != tarball extraction + tree validation (TV-02)
  != patch application with hash binding (TV-03)
  != compiled ARM32/ARM64 artifact (TV-06/07)
  != installed APK on device (D8 gate)
  != runtime execution receipt
```

Each missing link remains TOKEN_VAZIO/blocked for the corresponding claim.

## Coding discipline

- **Bounds checks:** Manifest offsets, buffer sizes, array indices must be explicit
- **Error paths:** NULL/error returns from lookup functions must be handled (not silenced)
- **Binary layouts:** Do not alter package metadata, manifest schema, or receipt formats silently
- **Reproducibility:** SOURCE_DATE_EPOCH must be set; output hashes recorded
- **ASM changes:** Golden tests required for SIMD/ARM-specific code
- **Gate failures:** Never suppress with `|| true`, unconditional success, or silent fallback

## Documentation discipline

When editing prose:

- Distinguish `REFERENCE`, `IMPLEMENTED`, `PASS`, `FAIL`, and `TOKEN_VAZIO`
- Bind PASS statements to the specific gate/commit/environment that produced them
- Label heuristic claims separately from executed-gate claims
- Update stale onboarding text when cycles/gates have superseded it
- Prefer canonical statements + links over duplicated instructions

## Cycle gates (reference)

Each cycle has explicit, runnable gates. Only run gates applicable to your environment.

### Cycle 1.5 (Hardening — current)
```sh
make syntax                    # Compile check
make compiler-contract         # Type/bounds validation
make cycle1-artifact-gate      # TAR reproducibility, ELF proof
```

### Cycle 2 (Source acquisition — TV-01 to TV-05)
```sh
make cycle2-source-gate        # TV-01: fetch + SHA-256 verify
make cycle2-extract-gate       # TV-02: tarball extraction + tree validation
make cycle2-patch-gate         # TV-03: patch application with hash binding
make cycle2-manifest-gate      # TV-04: Manifesto V2 offsets & bounds
make cycle2-dep-gate           # TV-05: dependency resolution from manifest
```

### Cycle 3 (Device validation — TV-06 to TV-10)
```sh
make device-d8-gate           # TV-06/07: Real ARM32/ARM64 build + readelf proof
```

**TOKEN_VAZIO:** CI observability (TV-13), device execution (TV-08-10), runtime (D8 gate)

## Useful entrypoints

```sh
# Ground truth on current state:
cat core/STATUS.md
cat AGENTS.md

# Navigation:
cat docs/00-INDEX.md

# Cycle gates (run only if environment supports):
make syntax
make compiler-contract
make cycle1-artifact-gate
```

## Handoff

Finish with:

```text
F_ok   = what was actually changed/executed/demonstrated
F_gap  = what remains unknown, blocked, contradicted or unexecuted
F_next = smallest reproducible next action
```

**Important:** Never invent merit to make F_ok look larger. TOKEN_VAZIO and FAIL remain visible.
