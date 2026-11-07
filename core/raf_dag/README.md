# RAFCODEPhi DAG V1 — authorial freestanding core

An isolated C11 graph engine: no hosted headers, libc, heap, syscalls, dynamic imports or global mutable data in raf_dag.c/h. Its caller supplies the bounded workspace. Compiler/linker are **build-time** prerequisites, not embedded runtime dependencies.

Model: prerequisite -> consumer. init / add / finalize stable CSR / solve topological order and longest depths. Error codes distinguish input, state, capacity and cycle. Limits: 2057 nodes, 14336 edges. Depth uses 16 bits; duplicate edges allowed. The object is caller-owned and must not be concurrently mutated without caller synchronization.

Test: make -C core/raf_dag verify CC=clang. The standalone test.c uses hosted stdio for diagnostics but **is not part of the freestanding product**. Build ARMv7a and AArch64 objects with -ffreestanding -fno-builtin -fno-stack-protector; nm -u should show no undefined symbols. Device runtime, APT publication and bootstrap installation remain TOKEN_VAZIO until proven.

Optimization: -O2, -ffunction-sections, -fdata-sections, -fvisibility=hidden and -Werror are compiler hints/checks, not proof of performance. Comments and ordinary formatting are not machine instructions; only directives/attributes/flags can affect code generation and those effects require emitted-binary inspection. No claim of bare metal Android without an OS interface.

FNV-based fingerprint is non-cryptographic; artifact custody requires a cryptographic digest outside this core. The original hosted builder and third-party recipes are left untouched, with their upstream authorship and licenses preserved. Legacy adapter remains a separate compatibility layer and requires its own tests. claim_allowed_device_runtime=false.

## V1.1 hardening

- Reuses indegree[] as CSR scatter cursor: 4,114 fewer bytes per caller workspace (3.86% of v1).
- Rejects malformed CSR offsets and adjacency before traversal; resets solved on failures.
- Adds independent seeded property tests (512 generated graphs), duplicate edges, a longest-depth oracle and negative structural tests.
- Preflights legacy layer capacity before populating any layer and tests depth overflow at 256 edges.
- Graph fingerprint is input-order-sensitive and non-cryptographic. No physical Android/apt/runtime claim.

## Selectable compiled-code profiles

Production **core only**: `make -C core/raf_dag verify CC=clang PROFILE=balanced` uses `-O2`; `PROFILE=compact` uses `-Oz`. Both retain freestanding/no-libc flags, strict warnings, the same C API and the same property tests. Hosted test executables are not distributed. CI reports the emitted object text bytes and requires compact text not exceed balanced for this pinned build. This is a size gate, **not** a CPU speed benchmark or proof of smaller Termux APK/bootstrap. A consumer must choose the profile explicitly and reverify its ABI and device behavior. No production-profile substitution occurs automatically.
