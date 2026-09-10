# Third-Party Provenance — Termux packages fork

Status: THIRD_PARTY_UPSTREAM_WITH_LOCAL_DELTAS / claim_allowed=false for whole-repository authorship.

## Upstream chain
- Source authority: `termux/termux-packages`
- Parent fork: `termux-play-store/termux-packages`
- Destination: `rafaelmeloreisnovo/termux-packages`
- Relationship: GitHub fork chain.

## Authorship boundary
The inherited package build system, recipes, patches and package metadata remain subject to their original upstream copyright and per-package license terms. Repository ownership or local edits do not create ownership of inherited content.

RAFAELIA/RMR authorship may be claimed only for independently added deltas with path/commit/blob evidence. Unknown lineage is `TOKEN_VAZIO`.

## License control
GitHub reports repository-level `NOASSERTION`; package recipes can refer to many distinct upstream licenses. Therefore no single repository-wide SPDX identifier is sufficient for the distributed package set.

Before distribution, resolve each affected package through: `package -> source URL/ref -> recipe/patch -> destination -> upstream license -> local delta -> attribution/NOTICE/source obligations -> evidence`.

This file does not replace any original LICENSE, NOTICE, copyright header, package metadata or source-offer obligation.

## Local provenance receipts

### 2026-09-10 — `packages/hello-world/build.sh`
- component/material: GNU Hello package recipe metadata
- source: `termux/termux-packages`, `packages/hello/build.sh` (observed current upstream recipe: GNU Hello 2.12.3)
- destination: `packages/hello-world/build.sh`
- adapted: homepage, license/maintainer fields, version, source URL, SHA-256, dependency/build flags and pre-configure linker flag
- upstream software license recorded by recipe: GPL-3.0
- local change: replaced an RFC-reserved `mirror.example.com` placeholder recipe with a pinned buildable GNU source recipe
- evidence: local commit `feedda1058fcc4db1570b8971f99a0c6e46f893a`; fleet rerun required for PASS
- gap: source fetch/build/install/runtime remain `TOKEN_VAZIO` until corresponding CI/device evidence exists
