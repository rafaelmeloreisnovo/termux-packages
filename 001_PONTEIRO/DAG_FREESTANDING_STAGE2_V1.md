# RAFCODEPhi DAG — Stage 2: real-source shadow

Source: rafaelmeloreisnovo/termux-packages, after merged PR185.
Authority: package/source producer, read-only verification only.
Default builder, pkg-real and upstream recipes are untouched.

## Semantic bridge

The hosted pkg_dag implementation reads actual build.sh recipes and records
consumer -> dependency. The freestanding engine expects dependency -> consumer.
The adapter explicitly inverts each edge and compares per-node longest depths,
the maximal depth, and the validity of the hosted topological order. Two valid
topological orders need not be identical; a cycle is BLOCKED_CYCLE, never PASS.

The authorial raf_dag.c is freestanding C11 with no libc, heap or syscalls.
Scanner, parser, CLI and pkg_dag are hosted platform-adapter code.
Existing third-party copyright, attribution and licenses are preserved.

## Execute

make -C core pkg-real-dag-shadow CC=clang
./core/pkg-real-dag-shadow <repo-root>

Bounded: up to 2057 nodes, 14336 projected edges; oversize is
BLOCKED_CAPACITY with no truncation. The static parser does not evaluate Bash
expansions or alternative providers; unresolved dependencies stay visible.
PASS_SCOPED is agreement of two algorithms on the same static projection only.

CI uses six *actual* package source recipes from this repository, with
all four scanner roots in a controlled temporary fixture. Also tests reverse
orientation, order permutations, depth errors, cycles and overflow.

Rollback: close or revert isolated Stage 2 PR. No automatic merge, package
replacement, APT publication or Android runtime claims.
