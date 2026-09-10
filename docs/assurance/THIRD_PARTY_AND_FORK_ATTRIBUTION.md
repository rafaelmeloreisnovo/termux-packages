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
| `termux-app-rafacodephi` | Termux team and contributors / `termux/termux-app` | fork README, GPLv3 declaration and file-specific notices | preserve Termux attribution and exceptions; consumer app, not a package recipe |
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
