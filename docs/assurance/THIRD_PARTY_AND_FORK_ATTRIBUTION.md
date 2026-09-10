# RAFCODEPHI — Third-Party and Fork Attribution

This document is the human-readable companion to
`docs/assurance/rafcodephi-federated-components.v1.json`.

## Principle

RAFCODEPHI does **not** claim authorship of upstream work merely because a fork
is hosted under `rafaelmeloreisnovo`. Original copyright, license, trademark
and attribution notices remain with their respective authors and projects.
Local modifications are identified as fork modifications; they do not erase or
replace upstream authorship.

`SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM` and `TOKEN_VAZIO != 0`.

## Components and credits

| Local component | Upstream / original credit | Governing evidence | Integration rule |
|---|---|---|---|
| `termux-app-rafcodephi` | Termux team and contributors / `termux/termux-app` | fork README, GPLv3 declaration and file-specific notices | preserve Termux attribution and exceptions; consumer app, not a package recipe |
| `BLAKE3` | Jack O'Connor, Samuel Neves and BLAKE3 contributors / `BLAKE3-team/BLAKE3` | `Cargo.toml`: `CC0-1.0 OR Apache-2.0 OR Apache-2.0 WITH LLVM-exception` | the distributed `b3sum` recipe continues to use official upstream source; the local fork is not silently substituted |
| `Vectras-VM-Android` | **xoureldeen**, original Vectras VM Android author, plus contributors | fork `LICENSE`, GPLv2 | integrate by versioned contract/artifact; preserve the explicit xoureldeen attribution |
| `qemu_rafaelia` | Fabrice Bellard, QEMU team and contributors | QEMU `LICENSE` / `COPYING` | path-aware licensing: QEMU as a whole is GPLv2, while individual files/TCG/firmware may carry compatible BSD/MIT/other terms |
| `androidx_RmR` | AndroidX / Jetpack contributors, Google / Android Open Source project | root `LICENSE.txt`, Apache-2.0 | no source transplant until the exact consumed module and upstream origin are bound |
| `termux-api_rafcodephi` | Termux team and contributors / `termux/termux-api` | fork `LICENSE.md`, GPLv3 provenance | Android companion app remains distinct from the `termux-api` CLI package |

## Mandatory source-origin comment for transplanted or modified external code

If external source is ever copied into a RAFCODEPHI-owned package, patch, C/ASM
module or generated source, the copy must retain the original notice when the
license requires it and add a bounded provenance note in the nearest sensible
file/header or patch metadata:

```text
Origin-Repository: owner/repository
Origin-Commit: 40-hex commit
Origin-Path: exact/path/in/source/tree
Original-Author-or-Project: name/project
Governing-License: SPDX expression or exact file-specific license
Local-Modification: concise description; no false authorship claim
```

For QEMU this rule is mandatory **per copied file** because the tree contains
file-specific licenses. A repository-level `GPL-2.0` label is not sufficient to
relicense or erase a BSD/MIT/file-specific notice.

## Package-recipe rule

Every distributed package path must keep its real package license, homepage,
source URL/ref and integrity pin when available. The repository-wide
`LICENSE.md` explicitly makes package recipes/patches follow the actual package
license, while build infrastructure outside package trees is Apache-2.0.

The RAFCODEPHI profile packages added by this change contain only RAFCODEPHI
profile metadata and dependency declarations; they do **not** vendor Termux,
BLAKE3, Vectras, QEMU, AndroidX or Termux:API source code. Their Apache-2.0
license therefore applies only to that new local metadata/package glue.

## QEMU ARM32 x86_64 recipe modification provenance

This record covers the local package-recipe change introduced by PR #110. It
is a packaging modification only; it does not claim authorship of QEMU source
and it does not transplant source from `qemu_rafaelia`.

| Field | Evidence / value |
|---|---|
| Component / material | `packages/qemu-system-x86-64-headless/build.sh` |
| Origin fork lineage | `rafaelmeloreisnovo/termux-packages` → parent `termux-play-store/termux-packages` → source `termux/termux-packages` |
| Origin local base | commit `93284c7df537a0f14a148b1839377f06aac2929f`; recipe blob `0fc6361b7b4da3de14e19a5fb5bf28c3c56721f2` |
| Destination | same path in branch `rafcodephi/qemu-arm32-x86_64-v1-20260910` |
| Third-party source fetched by recipe | official QEMU release from `https://download.qemu.org/`, pinned by existing SHA-256 |
| Upstream/original project credit | Fabrice Bellard; QEMU team and contributors; Termux package-maintainer lineage retained |
| Governing recipe license | `GPL-2.0`, because repository `LICENSE.md` applies each package's license to scripts and patches under the package tree and this recipe declares `TERMUX_PKG_LICENSE="GPL-2.0"` |
| Local modification | add package revision; request `x86_64-softmmu` for `TERMUX_ARCH=arm`; fail closed when the ARM32 install tree lacks `bin/qemu-system-x86_64` |
| New CI/build-infrastructure file | `.github/workflows/rafcodephi-qemu-arm32-x86_64.yml`; Apache-2.0 under repository build-infrastructure rule |
| Runtime claim | `TOKEN_VAZIO`; structural build/package evidence is distinct from physical Android execution |
| Required follow-up | preserve QEMU/Termux notices in distributed artifacts and keep physical-device runtime behind its own receipt/gate |

No QEMU C/ASM/firmware source file is copied or modified by this package-recipe
change. If that boundary changes, per-file QEMU license provenance becomes a
new mandatory gate before distribution.

## Redistribution dignity

When a third-party license requires a license copy, NOTICE text, source offer,
modified-file notice or preservation of copyright statements, the corresponding
artifact/release must carry those obligations. SPDX metadata and this document
are indexes; they do not replace the complete license text or a required NOTICE.

No upstream project, author or contributor is represented as endorsing
RAFCODEPHI merely because their open-source work is used or referenced.

## Evidence boundary

This attribution layer is a structural compliance control, not a blanket legal
warranty and not runtime evidence. Build, installation, physical Android
execution, signing and release claims retain their independent gates.
