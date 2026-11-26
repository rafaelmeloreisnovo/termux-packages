# RAFAELIA MicroASM v2 per-unit build

36 physical leaf .c sources (12 operations in portable, armv7 and aarch64). Compiling armv7 twice (A32 + Thumb-2) produces 48 separate ELF objects and 4 ABI/mode archives. Exactly one exported function per file.
Build: sh raf_microasm/v2/tooling/build_v2.sh /tmp/raf_microasm_gate
Build tools cc, clang, nm, llvm-ar, llvm-objdump are build-time requirements only. Header/include and leaf sources are freestanding (no heap, libc, JNI or syscalls).
Bounded file-specific compiler settings in flags/units.v2.tsv: source + ABI + optimization -O2/-O3/-Os. Static parser rejects unknown or inconsistent sources; no eval. Never co-link competing ABI/mode archives.
ARM targets: armv7a-linux-androideabi21 A32/Thumb2 and aarch64-linux-android21, compiled as PIC. Host pure C baseline vectors include boundary cases, 1000 pseudo-random cases and negative rejection.
Per-object symbol closure and target ISA disassembly are checked, but current gate is not physical ARM code execution or Android installation.
Runtime integration, Ninja, bootstrap ZIP, dpkg, APT, APK, device startup are untouched. A compiler toolchain is needed for builds even though produced pure-core modules have zero runtime library requirements.
Do not infer speedup from more files; function call/link overhead must be measured with physical device benchmarks and a compiler-generated reference.
Rollback: close DRAFT PR or review a scoped successor revert; preserve licenses, upstream third-party modules and receipts.
SOURCE != ARTIFACT != EXECUTION != EVIDENCE != CLAIM. claim_allowed=false for device or release.
