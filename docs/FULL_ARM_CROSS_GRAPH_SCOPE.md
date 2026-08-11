# RAFCODEPHI Full ARM Cross Graph — evidence boundary

This gate closes a compile-coverage gap left by subset-oriented ARM/QEMU gates.

It verifies the complete default `core` build graph selected for non-x86 compilers on both ARM32 and AArch64, using real GNU cross toolchains and cross libc development headers.

Invariant:

`full default graph -> clean build -> target ELF identity -> receipt`

Fail-closed rule:

`ANY_ARCH_RED => TRANSIT_BLOCKED`

Epistemic boundary:

- `claim_allowed=false`
- `physical_android=TOKEN_VAZIO`
- Linux GNU cross compilation is not Android/Bionic physical execution.
- A green result proves compile/link portability of the selected default graph only.
