# AGENTS.md — RAFAELIA / termux-packages

## Federation entry

This repository is the RAFAELIA **package/source factory**. Enter through indices and contracts, not through a broad repository crawl or remembered prompt.

Federated routing/state authority: `rafaelmeloreisnovo/Mapa`.  
Control-plane executor contract: `rafaelmeloreisnovo/RafGitTools:configs/agent-entry-kernel.v1.json` when cross-repository access is available.

### Mandatory service preflight — Q01..Q12

Before mutating or promoting state, answer with exact pointers or typed `TOKEN_VAZIO`:

1. **Quem sou?** — package/source-factory agent + local repository role.
2. **Qual repo/ref/path/hash estou lendo?** — repo/ref/exact commit/recipe/path/source or artifact identity.
3. **Qual minha autoridade?** — termux-packages owns source/recipe/package handoff implementation; consumer runtime belongs to its producer repo; `Mapa` owns federated route/state.
4. **Qual minha fronteira?** — source/build/package claims are distinct from install/runtime claims.
5. **Quais índices locais devo abrir?** — minimum navigation, architecture, audit, recipe and relevant workflow only.
6. **Qual rota do Mapa corresponde ao objetivo?** — explicit route/anchors or typed `TOKEN_VAZIO`.
7. **Que lacunas já existem?** — staged handoff gaps, TOKEN_VAZIO, uncertainties and consumer dependencies.
8. **Qual evidência é atual?** — exact source commit, recipe, build artifact digest, schema and receipt scope/staleness.
9. **Qual gate posso executar?** — source verification/build/schema/handoff fixture with falsifier, exit and rollback.
10. **Quando devo parar?** — stop on authority/dependency/privacy/security block, observed exit, or no marginal reconstruction gain.
11. **Onde registro o delta?** — local package/handoff receipt; route material cross-repo state to `Mapa`; Drive only for durable reconstruction changes.
12. **Quais regras de governança, dados, privacidade e segurança governam esta unidade?** — classify all four before mutation.

### Local governance/data/privacy/security defaults

- **Governance:** package recipes/source pins are local authority; consumer install/runtime state cannot be promoted from this repo alone. High/critical mutation requires rollback.
- **Data:** source archives, recipes, manifests, build logs and artifacts must have explicit identity/schema. Public upstream source does not make all local logs/config PUBLIC.
- **Privacy:** build receipts should contain hashes, versions and bounded metadata, not credentials, home paths, user data or unrelated environment values. Unknown sensitivity blocks public publication.
- **Security:** source authenticity/integrity, download URLs/checksums, build scripts, signing boundaries, secrets, archive extraction and producer/consumer schemas are security surfaces. Invalid schema/checksum or unknown critical classification fails closed.

Sequence:

`bind exact commit → authority/boundary → local indices → Mapa route → open gaps → service classification → deterministic F_next → baseline/rollback → narrow mutation → producer/consumer validation → receipt → append → recompute`

## Local role and entry routes

Role: turn pinned sources/build recipes into package/prefix artifacts with reproducible handoff metadata for consumers such as `termux-app-rafacodephi`.

Start with:

- `docs/00-INDEX.md`
- `docs/01-NAVIGATION.md`
- `docs/02-ARCHITECTURE_MAP.md`
- `docs/04-AUDIT_AND_EVOLUTION.md`
- `build-package-rafcodephi.sh`
- the concrete package build recipe/source files touched by the task
- relevant workflow/validator for the exact package boundary

If those reconstruct the target and more history cannot change the gate/evidence/provenance/privacy/security classification, stop crawling and run the bounded gate.

## Package-factory invariants

- Source provenance must be explicit before a package artifact can support a runtime claim.
- A generated property/manifest receipt must match the exact bytes consumed by the build.
- Build success does not prove install/runtime success in `com.termux.rafacodephi`.
- Documentation-only handoff descriptions are not executable handoff evidence.
- REAL/BITRAF/PHASE or other staged outputs must not cross a boundary without a versioned producer/consumer schema once that boundary is used operationally.
- Fail-closed behavior must be explicit; silent continuation across an invalid stage is a gap.
- Package, ABI, source commit, artifact digest and consumer expectation must be linkable in the handoff receipt.
- Physical Android installation/pkg execution remains `TOKEN_VAZIO` until a device receipt exists.
- No credential/token may be embedded in source URL, recipe, artifact manifest or public receipt.

## Current structural gap focus

The documentation audit identifies unresolved handoff questions around staged REAL → BITRAF64 → PHASE9 flows, output schemas, error decisions and end-to-end examples. Treat these as `UNCERTAIN`/`P1` until current executable code proves a stronger state.

Do not promote an old documentation observation directly to a current runtime failure; first bind it to the current commit and inspect the producer/consumer implementation.

## Deterministic work selection

Within an urgency class:

1. repair a broken source/provenance/security or build blocker;
2. define/validate an upstream output schema before changing a downstream consumer;
3. prefer a reproducible fixture with exact hashes over prose-only clarification;
4. prefer one end-to-end representative package path before multiplying examples;
5. preserve newly discovered gaps in the ledger instead of weakening a validator.

## Evidence receipt

For a handoff-producing action, record at minimum:

- source repository/ref/commit;
- recipe/package identity;
- architecture/ABI;
- input/source digests;
- output artifact digest;
- schema/version at each crossed boundary;
- validator/test result;
- consumer expected identity;
- governance/data/privacy/security classification;
- falsifier, exit criterion and stop reason;
- `F_ok`, `F_gap`, `F_next`;
- rollback reference;
- `claim_allowed` boundary.

## Boundaries

`TOKEN_VAZIO != 0`; urgency is not confidence; `READY_TO_TEST != RESOLVED`; build receipt != install receipt != runtime receipt. Historical observations are append-only and evidence does not silently transfer to a new commit. Do not publish raw environment/secrets when a bounded hash/reference is sufficient.
