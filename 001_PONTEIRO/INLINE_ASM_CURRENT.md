# RAFAELIA Ω — INLINE ASM CURRENT — L0

Status: DRAFT/HOLD; local host selftest + compile-only ARMv7-A32/Thumb2/AArch64 PASS_SCOPED; exact-head GitHub CI and physical ARM runtime TOKEN_VAZIO_NOT_RUN.

Source: rafaelmeloreisnovo/termux-packages, branch feat/inline-asm-dualabi-l0-20261107; companion repo has same API and should preserve byte-identical header (verify hashes independently). `core/raf_inline_asm_v1.h` implements 32-bit REV, CLZ and RBIT+CLZ without libc, heap, syscalls, JNI or runtime dependencies.

Test: `sh tests/raf_inline_asm_v1_gate.sh core`; strict warnings, positive vectors, deliberately failing negative vector, three ELF codegen targets, ISA disassembly and zero undefined symbols. Build-time cc/clang/readelf/nm/llvm-objdump are tools, not runtime dependencies.

Package producer: no recipe, dpkg, APT, bootstrap builder or upstream source is replaced. No runtime behavior change from these additive files. ARM32 `apkc/sys.h::os_open` has a separate possible mode-argument gap to audit; not changed here.

Next: exact-head CI → peer parity SHA256 → review → scoped consumer wiring → real package/ZIP/APK pinned SHA → ARMv7 physical bootstrap+pkg/apt+terminal receipts. `claim_allowed=false`, `release_allowed=false`.

Rollback: close draft PR; no main mutation, deletes, force push or license changes. Upstream licenses and notices remain intact.

SOURCE ≠ ARTIFACT ≠ EXECUTION ≠ EVIDENCE ≠ CLAIM.
