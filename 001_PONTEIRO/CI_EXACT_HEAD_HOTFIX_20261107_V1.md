# RAFAELIA Ω — CI exact-HEAD causal hotfix (Packages, 2026-11-07)
μID=MU-TERMUX-PACKAGES-EXACT-HEAD-20261107-V1
source=rafaelmeloreisnovo/termux-packages/pull/187 | parent=eef07618af10078535ac19e58979e51dacbb78bc | kind=CAUSAL_CI_HOTFIX+APPEND_ONLY_RECEIPT | routes=P,C,R,I,E
facts=At prior HEAD eef07618af10078535ac19e58979e51dacbb78bc, inline ASM run 42426800173 failed exit 127 ('llvm-objdump: not found'); syntax run 42426800277 failed missing bounded timeout in raf-inline-asm-l0.yml; MicroASM v2 run 42426800250 SUCCESS_SCOPED.
delta=Exact PR HEAD checkout with no persisted credentials; add bounded timeouts to L0 and V2; resolve versioned LLVM objdump or install llvm-18 on Ubuntu 24.04 test runner only; fail closed if unavailable; disassemble actual ARMv7/A64 objects, require rev/clz/rbit and reject negative vector. No source-core/production-package/recipe change; no policy disable, no merge.
gate=CI_NEW_HEAD:TOKEN_VAZIO_NOT_RUN; LIVE_ARM32/ARM64:TOKEN_VAZIO_NOT_RUN; BOOTSTRAP_APT_PKG:TOKEN_VAZIO_NOT_RUN; CLAIM_ALLOWED=false; RELEASE_ALLOWED=false
evidence=provider prior run logs 42426800173 and 42426800277; artifact build run 42426800250; verify exact new HEAD workflow checks after push; source file blob readback required.
rollback=revert single scoped branch commit in follow-up reviewed commit, preserve receipt/history; no force push or main mutation.
next=read new exact-head runs; separate hardware/consumer gate from object-only CI.
R3=<targeted tool/timeout/checkout patch, new-run and handset gaps, exact-head CI then physical consumer>
