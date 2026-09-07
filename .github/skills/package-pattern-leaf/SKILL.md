---
name: package-pattern-leaf
description: Apply a reusable architecture pattern to the package/source-factory role without crossing into install or runtime claims. Use when a prior language, database, build system, or legacy architecture suggests a better recipe, handoff, validation, integrity, or error-state mechanism.
---

# Package / Source Pattern Leaf

Read `AGENTS.md` first. This skill cannot expand package/source authority.

## Local projection

Translate an extracted pattern only into source/recipe/handoff behavior:

```text
source mechanism
-> recipe/source invariant
-> explicit inputs + guards
-> deterministic build/handoff action
-> artifact/schema identity
-> producer-side falsifier
-> receipt
```

Useful transfers may include:

- Pascal-like explicit type/range/initialization discipline -> schema fields, numeric bounds and deterministic recipe state;
- InterBase-like trigger/integrity discipline -> explicit stage activation, transaction-like handoff guards, durable bounded logs and failure decisions.

Do not import syntax or runtime authority from the source technology.

## Preserve

- source provenance and pinned identity;
- `armeabi-v7a` wherever locally declared;
- package/ABI/source/artifact linkage;
- fail-closed schema/checksum decisions;
- `TOKEN_VAZIO` for install/device/runtime evidence not produced here.

## Forbidden transfer

```text
build success != install success
package artifact != device runtime
reference pattern != proof
skill != authority
```

Never copy private derivation text into this public repository. A private seed may provide only the sanitized mechanism.

## Completion

Record `source_pattern`, `local_recipe_leaf`, `falsifier`, `F_ok`, `F_gap`, `F_next`, rollback and `claim_allowed=false` unless a separate bounded gate promotes the exact package/source claim.
