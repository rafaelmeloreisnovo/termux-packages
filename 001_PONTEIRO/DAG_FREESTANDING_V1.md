# 001_PONTEIRO — DAG freestanding V1

Producer: rafaelmeloreisnovo/termux-packages.
Branch: fix/raf-dag-csr-freestanding-20261107 (review only; main untouched).
Source: core/raf_dag/raf_dag.h + raf_dag.c.
Legacy API patch: core/dep_resolver_freestanding.c.
Hosted tests: core/raf_dag/test.c + core/tests/test_raf_dag_legacy.c.
Gate: .github/workflows/rafcodephi-dag-freestanding.yml.

Contract: prerequisite -> consumer edges, stable CSR, deterministic topological traversal, cycle rejection, explicit bounds, no heap and no libc/syscalls inside the independent kernel. Fixed limits: 2057 nodes and 14336 edges, depth uses 16 bits. The legacy adapter still has uint8_t depths and 65 nodes per layer; overflow returns an error, never silent truncation. Legacy adapter has one global workspace and is not reentrant.

SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM.
Local host/compiler evidence is scoped. CI provider status and Moto E7 physical runtime are separate gates.
No bootstrap/APT/device proof here. claim_allowed_device_runtime=false.
Rollback: revert review branch without merge, or revert only commits affecting these explicit paths. Never modify inherited third-party licenses.
