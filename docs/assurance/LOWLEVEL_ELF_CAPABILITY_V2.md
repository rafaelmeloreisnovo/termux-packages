# Native object / hardware capability gate V2

Parent main: `0a70700697b3a6a0ba2961f51189189e5305903a`.
Review branch only. No package recipe, source pin, published package, Android
bootstrap or install is changed. Existing v1 assurance manifests remain history.

| Item | Evidence / change | Boundary |
| --- | --- | --- |
| CPU feature observation | Previous union across CPU records could report a feature absent on another core | Intersection; a missing/empty core feature record is unknown |
| Native dispatch | `/proc/cpuinfo` alone cannot authorize execution of optional ISA | Explicit `native_dispatch_authorized=false`; OS HWCAP/ABI/device gates still required |
| Pure objects | Existing portable and ISA-specific `rafop_rbit32`, compiled separately for Android ARMv7/API21 and AArch64/API21 | Four real ET_REL files; exact class/machine/EABI and named global function, zero undefined symbols |
| Syscalls | Compiler assembly for these four primitives checked for calls/syscalls | No syscall injected; final linked executable/disassembly remains a separate gate |
| Negative controls | Wrong ABI, missing symbol, bad header, truncation and actual hosted external dependency rejected | No execution of mutated ELF |
| Consumer | Termux app PR #545 Java/DEX gate | Cross-repo install/runtime receipt remains TOKEN_VAZIO_NOT_RUN |

The ELF gate is an ABI/symbol-closure check, not a general loader, complete ELF
validator, linker, instruction-set verifier, security proof or speed measurement.
An object is not a process. Pure compute remains syscall-free; process entry,
exit, memory mapping, files and clocks belong to PLATFORM_GATE/HOSTED_BOUNDARY.
Using every unit of hardware is not an optimization metric: retain scalar and
portable paths; activate SIMD/GPU only after a feature gate and comparable workload
measurements show a benefit. Current Java SWAR-count and native RBIT gates exercise
different operations and must not be used for a cross-language speed ratio.

```sh
CLANG=clang bash scripts/verify-lowlevel-objects.sh . /tmp/raf-native-new-output
python3 core/tests/test_arch_runtime_probe.py
```

Hosted build dependencies: existing Clang, readelf, Bash, Python 3 and sha256sum;
no added runtime library or copied external implementation. Local compiler:
Debian Clang 19.1.7; flags and exact artifact hashes are emitted by the script.
The source primitives and their existing licenses/provenance are unchanged.
CPU records are an observation snapshot; hotplug, OS vector state and HWCAP are
not proven by their intersection. Missing features are never replaced by typical
CPU/cache assumptions. No affinity, governor, root access or kernel changes.

Governance/data/privacy/security: user-authorized reversible producer source
change; public code + synthetic tests only; no identifiers, credentials or raw
device logs. Origin/topology/evidence routing follows producer → object → receipt
→ consumer; consumer execution is an open edge. No new distribution, replication,
query language, compression, toroidal or RLL implementation is needed for this gate.

F_ok: four cross-compiled objects, ELF negative tests, CPU-probe regressions.
F_gap: exact-head CI, full linked package, consumer/device, performance comparison,
independent review. F_next: run these gates on the exact branch before a reviewed
handoff. Rollback: scoped revert successor; preserve all previous receipts.
`claim_allowed=false`; build evidence never authorizes a release or merge.
