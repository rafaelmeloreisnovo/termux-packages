#ifndef REAL_ARCH_H
#define REAL_ARCH_H

/*
 * REAL: architecture matrix with absolute auto-adaptation.
 * Status: REAL — enum, compile-time detection, runtime detection, and
 * property matrix are all real data derived from compiler predefines
 * (compile-time) or uname(2) (runtime). No simulation, no guessing.
 *
 * Supported architectures (15 total):
 *   x86_64, i386, arm64, arm32, riscv64, riscv32, mips64, mips32,
 *   ppc64le, ppc32, s390x, sparc64, loongarch64, wasm32, arm64_darwin
 *
 * arm64_darwin is treated as a distinct target because its OS (Apple)
 * and syscall convention differ from Linux aarch64; sharing hardware
 * with Linux arm64 does not mean sharing ABI.
 *
 * Auto-adaptation surface: property matrix returned by real_arch_props()
 * carries word size, endianness, page size, cache-line size, SIMD flags,
 * syscall convention. Any consumer can select behavior from these.
 *
 * Compatibility matrix: real_arch_compat(a, b) returns a bitmask of
 * REAL_COMPAT_* flags describing whether A can run B's code, share ABI,
 * share endianness, share word size, or emulate B.
 */

#include "real_attrs.h"
#include <stdint.h>
#include <stdio.h>

/* Enum values are STABLE: existing values must never be renumbered. */
typedef enum {
  REAL_ARCH_UNKNOWN       = 0,
  REAL_ARCH_X86_64        = 1,
  REAL_ARCH_I386          = 2,
  REAL_ARCH_ARM64         = 3,   /* Linux aarch64 (LP64) */
  REAL_ARCH_ARM32         = 4,   /* Linux armv7/armhf (ILP32) */
  REAL_ARCH_RISCV64       = 5,
  REAL_ARCH_RISCV32       = 6,
  REAL_ARCH_MIPS64        = 7,
  REAL_ARCH_MIPS32        = 8,
  REAL_ARCH_PPC64LE       = 9,   /* IBM POWER little-endian */
  REAL_ARCH_PPC32         = 10,
  REAL_ARCH_S390X         = 11,
  REAL_ARCH_SPARC64       = 12,
  REAL_ARCH_LOONGARCH64   = 13,
  REAL_ARCH_WASM32        = 14,
  REAL_ARCH_ARM64_DARWIN  = 15,  /* Apple Silicon macOS/iOS */
  REAL_ARCH__COUNT
} real_arch_t;

typedef enum {
  REAL_ENDIAN_LITTLE = 0,
  REAL_ENDIAN_BIG    = 1
} real_endian_t;

/* SIMD/vector flags — bitmask, extensible. */
#define REAL_SIMD_NONE     0x00
#define REAL_SIMD_MMX      0x01
#define REAL_SIMD_SSE2     0x02
#define REAL_SIMD_AVX2     0x04
#define REAL_SIMD_AVX512   0x08
#define REAL_SIMD_NEON     0x10  /* ARM Advanced SIMD */
#define REAL_SIMD_SVE      0x20  /* ARM SVE */
#define REAL_SIMD_RVV      0x40  /* RISC-V V */
#define REAL_SIMD_ALTIVEC  0x80  /* POWER AltiVec */

/* Syscall conventions — every supported OS/arch pair has one. */
typedef enum {
  REAL_SYSCALL_LINUX_SYSCALL = 0,  /* Linux `syscall` instruction */
  REAL_SYSCALL_LINUX_SVC     = 1,  /* Linux ARM SVC */
  REAL_SYSCALL_DARWIN_SVC    = 2,  /* macOS/iOS `svc #0x80` */
  REAL_SYSCALL_WASI          = 3,  /* WebAssembly WASI imports */
  REAL_SYSCALL_UNKNOWN       = 255
} real_syscall_conv_t;

/* Compatibility flags — bitmask returned by real_arch_compat(). */
#define REAL_COMPAT_SAME        0x01  /* identical arch */
#define REAL_COMPAT_ABI         0x02  /* shares ABI (can call each other) */
#define REAL_COMPAT_ENDIAN      0x04  /* same endianness */
#define REAL_COMPAT_WORDSIZE    0x08  /* same word size */
#define REAL_COMPAT_ISA_SUPER   0x10  /* A is a superset of B (e.g. x86_64⊃i386) */
#define REAL_COMPAT_EMULATABLE  0x20  /* A can emulate B (qemu-user etc.) */

/* Property record — every field REAL, derived from arch enum. */
typedef struct {
  real_arch_t arch;
  const char *name;              /* canonical: "x86_64", "arm64", ... */
  const char *canonical_uname;   /* what uname -m returns */
  const char *termux_name;       /* what termux calls it (or NULL) */
  uint8_t word_bits;             /* 32 or 64 */
  real_endian_t endian;
  uint32_t page_size;            /* bytes, typical */
  uint32_t cache_line;           /* bytes, typical */
  uint32_t simd;                 /* REAL_SIMD_* bitmask */
  real_syscall_conv_t syscall_conv;
} real_arch_props_t;

/* Compile-time detection — evaluates to a stable arch id at cpp time. */
#if defined(__x86_64__) || defined(_M_X64)
#  define REAL_ARCH_BUILD REAL_ARCH_X86_64
#elif defined(__i386__) || defined(_M_IX86)
#  define REAL_ARCH_BUILD REAL_ARCH_I386
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
#  define REAL_ARCH_BUILD REAL_ARCH_ARM64_DARWIN
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define REAL_ARCH_BUILD REAL_ARCH_ARM64
#elif defined(__arm__) || defined(_M_ARM)
#  define REAL_ARCH_BUILD REAL_ARCH_ARM32
#elif defined(__riscv) && (__riscv_xlen == 64)
#  define REAL_ARCH_BUILD REAL_ARCH_RISCV64
#elif defined(__riscv) && (__riscv_xlen == 32)
#  define REAL_ARCH_BUILD REAL_ARCH_RISCV32
#elif defined(__mips__) && defined(__mips64)
#  define REAL_ARCH_BUILD REAL_ARCH_MIPS64
#elif defined(__mips__)
#  define REAL_ARCH_BUILD REAL_ARCH_MIPS32
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
#  define REAL_ARCH_BUILD REAL_ARCH_PPC64LE
#elif defined(__powerpc__) || defined(__PPC__)
#  define REAL_ARCH_BUILD REAL_ARCH_PPC32
#elif defined(__s390x__)
#  define REAL_ARCH_BUILD REAL_ARCH_S390X
#elif defined(__sparc__) && defined(__arch64__)
#  define REAL_ARCH_BUILD REAL_ARCH_SPARC64
#elif defined(__loongarch64) || defined(__loongarch_lp64)
#  define REAL_ARCH_BUILD REAL_ARCH_LOONGARCH64
#elif defined(__wasm32__)
#  define REAL_ARCH_BUILD REAL_ARCH_WASM32
#else
#  define REAL_ARCH_BUILD REAL_ARCH_UNKNOWN
#endif

/* Get the properties for a given architecture. Returns a pointer into
 * a static table; callers must not modify. Returns NULL for
 * REAL_ARCH_UNKNOWN or out-of-range. */
REAL_PURE
const real_arch_props_t *real_arch_props(real_arch_t arch);

/* Return the compile-time detected architecture (from REAL_ARCH_BUILD). */
REAL_CONST
real_arch_t real_arch_compile_time(void);

/* Detect the running architecture at run time via uname(2).
 * Returns REAL_ARCH_UNKNOWN if uname fails or name is unrecognized. */
REAL_WARN_UNUSED
real_arch_t real_arch_detect_runtime(void);

/* Compatibility matrix: return REAL_COMPAT_* bitmask describing what
 * relationships arch A has with arch B. */
REAL_PURE
uint32_t real_arch_compat(real_arch_t a, real_arch_t b);

/* Convert enum ↔ canonical name. Returns NULL on unknown. */
REAL_PURE
const char *real_arch_name(real_arch_t arch);

REAL_PURE REAL_NONNULL_ALL
real_arch_t real_arch_from_name(const char *name);

/* Emit the full matrix as JSON to `out`. */
REAL_COLD REAL_NONNULL_ALL
void real_arch_write_json(FILE *out);

/* Human-readable report. */
REAL_COLD REAL_NONNULL_ALL
void real_arch_report(FILE *out);

#endif /* REAL_ARCH_H */
