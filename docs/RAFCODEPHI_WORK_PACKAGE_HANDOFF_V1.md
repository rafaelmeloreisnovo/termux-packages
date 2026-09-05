# RAFCODEΦ WORK v1 — package producer/consumer handoff

Decision: `DEC-RAFCODEPHI-WORK-V1-20260905`
State: `PACKAGE_PROFILE_CANDIDATE`
Release allowed: `false`

## Purpose

This repository is the package producer candidate for the first usable `RAFCODEPHI_WORK-v1` environment.

The goal is not to declare the full RAFCODEΦ platform finished. The goal is to close the smallest reproducible chain that gives the app a real package stack on Android.

## Producer → consumer chain

`termux-packages@pinned_commit → bootstrap/repository metadata → termux-app-rafacodephi@pinned_commit → physical Android → runtime receipt`.

The consumer must never resolve a release from an unspecified floating branch state.

## Required minimum profile

The initial profile is defined in `profiles/rafcodephi-work-v1.json` and includes shell/core tools plus package management, git, clang toolchain, Python and basic transfer/archive utilities.

Presence in the profile is not proof that a package is available, built or runnable. Each package must resolve to a recipe/output and then to device evidence.

## Acceptance layers

1. `SOURCE_OBSERVED`: recipe/path exists at pinned commit.
2. `BUILT`: package artifact produced with build receipt.
3. `INDEXED`: package is included in repository metadata with hash.
4. `FETCHED`: device retrieves declared metadata/artifact.
5. `INSTALLED`: dpkg/apt/pkg install succeeds.
6. `EXECUTED`: designated runtime probe succeeds.
7. `WORK_APPROVED`: previous layers are bound to one lineage and no blocking contradiction remains.

## Required identity fields

For each promoted package occurrence record:

- producer repository;
- producer commit SHA;
- recipe path;
- package name/version;
- architecture/ABI;
- artifact hash;
- repository metadata hash;
- consumer app commit;
- device/runtime receipt.

`filename != identity` and `package name != artifact identity`.

## Current blockers

- `TV-WORK-PKG-PROFILE-AVAILABILITY`: package-by-package recipe/build coverage not yet enumerated for the pinned producer commit.
- `TV-WORK-BOOTSTRAP-BUILD`: no WORK-v1 bootstrap candidate with pinned-source manifest/hash receipt recorded here yet.
- `TV-WORK-REPOSITORY-PUBLISH`: no WORK-v1 runtime-accessible repository snapshot and device fetch receipt recorded here yet.

All remain `claim_allowed=false`.

## Immediate probes

1. Resolve every required package in the profile to its exact source path at the pinned commit.
2. Classify each as `SOURCE_OBSERVED | BUILT | TOKEN_VAZIO | BLOCKED`.
3. Build the smallest bootstrap/package-manager closure first: `bash/coreutils/termux-tools/dpkg/apt` plus dependencies.
4. Generate manifest and hashes.
5. Hand the exact producer commit and bootstrap hash to the app WORK branch.
6. Execute `dpkg`, `apt`, `pkg update` and a bounded `pkg install` on physical Android.

## Boundary

Freestanding probes, research packages and architecture experiments may continue in EVOLUTION, but they do not enter WORK-v1 unless they are dependencies of the minimum profile and pass the same evidence chain.
