# RAFCODEΦ · Fan-out/fan-in por lotes ARM / AArch64 — V2

**Authority:** `rafaelmeloreisnovo/termux-packages` is the producer of source-built bootstrap bytes. The Android APK is built by `rafaelmeloreisnovo/termux-app-rafacodephi`, outside this workflow. This hotfix does not merge them into a single unverified artifact.

## Why this route

The already merged V1 pipeline builds **both architectures in one expensive producer stage**. Spawning two independent builds would duplicate toolchain startup, downloads, cache contention and CI cost. Instead, V2 keeps one existing source build and validates its sealed architecture artifacts in **two parallel, read-only batches**, each with its own receipt, then joins both results before any cross-repository handoff.

```text
contract (fast tests, no native package build)
   |
produce (one hosted source build of arm,aarch64)
   |-> producer ZIPRAF (non-APK evidence + SHA-256)
   |-> existing immutable producer handoff artifact (unchanged)
   |
   +-------------------------+
   |                         |
verify-batch / arm       verify-batch / aarch64
   |                         |      max-parallel=2, fail-fast=false
   +------------+------------+
                |
         join-batches
         both receipts PASS / same SHA + run + artifact + manifest
                |
             handoff
         repository_dispatch to Android consumer
                |
           APK build (another repo, separate receipts)
```

**Preconditions:** `contract=PASS`, `produce=PASS`, both `verify-batch` matrix jobs PASS, `join-batches=PASS`. Each batch downloads the **same already uploaded** producer artifact; it does not rebuild or download the package toolchain. Missing, malformed or mismatched expected SHA, source ref, `GITHUB_RUN_ID`, artifact ID, artifact digest, archive CRC, arch-specific `BOOTSTRAP_PROFILE.json`, or release/claim boundary fails that job. If either fails or is canceled, join and handoff cannot succeed.

**Scoped status:** ARM receipt `rafcodephi.producer-arch-batch/v1` and AArch64 receipt of the same schema carry `scope=PRODUCER_ARCHIVE_BYTE_AND_PROFILE_ONLY`. Join receipt `rafcodephi.producer-arch-batch-join/v1` carries `scope=BOTH_PRODUCER_ARCHIVE_BATCHES_ONLY`. These do **not** assert Android install, native execution, APT runtime, science-grade benchmark, rights clearance, or release. `physical_android=TOKEN_VAZIO`, `claim_allowed=false` remain unchanged.

## Artifact contract

- `rafcodephi-producer-zipraf-<sha>`: original deterministic non-APK ZIPRAF + digest, **preserved**.
- `rafcodephi-termux-packages-<sha>`: original handoff artifact with the same paths/name and original repository_dispatch payload, **preserved**.
- `rafcodephi-batch-<run>-arm`: read-only ARM batch JSON receipt.
- `rafcodephi-batch-<run>-aarch64`: read-only AArch64 batch JSON receipt.
- `rafcodephi-batch-join-<run>`: combined manifest / SHA256SUMS.txt / both receipts.
- `*.apk`: **never** included here. APK signed/unsigned/ABI outputs and installed-byte proofs remain in the consumer repository.

**Failure/rollback:** replay only failed read-only verifier jobs when possible; do not trigger more full source builds while the queue is congested. Preserve original failed logs, binary bytes and receipts. Revert V2 workflow+verifier+test+documentation to V1 stages without modifying the legacy handoff schema; consumers are not migrated by this patch. Never claim a CI queued result as PASS.

**Quality and IP P0:** maintain source→artifact→execution→evidence→claim separation; apply package-by-package license review before redistribution; source build does not transfer third-party copyright. Cross-repo dispatch token remains in the last handoff job and never enters the batch or ZIPRAF.

**Proof still required:** exact-HEAD GitHub YAML parser/test outputs and batch self-test PASS; production source build + matrix receipt + join receipt + dispatch success from the same run. On PR, production jobs are intentionally `NOT_RUN` and cannot be promoted by syntax tests.
