#ifndef REAL_ARCH_H
#define REAL_ARCH_H

/*
 * Architecture identity catalog + NOMINAL reference matrix.
 * Status: OBSERVED_LIMITED.
 *
 * Compile-time architecture identity comes from compiler predefines and runtime
 * architecture identity comes from uname(2). The static property table is a
 * reference profile only: page size, cache line, SIMD support and emulator
 * availability are environment/device facts and MUST NOT be promoted from this
 * table to runtime evidence.
 *
 * Supported identifiers (15 total):
 *   x86_64, i386, arm64, arm32, riscv64, riscv32, mips64, mips32,
 *   ppc64le, ppc32, s390x, sparc64, loongarch64, wasm32, arm64_darwin
 *
 * Runtime capability evidence is produced separately by:
 *   scripts/arch_runtime_probe.py
 *
 * claim_allowed=false until the claimed target has an execution receipt.
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
  REAL_ARCH_PPC64LE       = 9,
  REAL_ARCH_PPC32         = 10,
  REAL_ARCH_S390X         = 11,
  REAL_ARCH_SPARC64       = 12,
  REAL_ARCH_LOONGARCH64   = 13,
  REAL_ARCH_WASM32        = 14,
  REAL_ARCH_ARM64_DARWIN  = 15,
  REAL_ARCH__COUNT
} real_arch_t;

typedef enum {
  REAL_ENDIAN_LITTLE = 0,
  REAL_ENDIAN_BIG    = 1
} real_endian_t;

/* SIMD/vector reference flags. These are nominal table metadata; consumers
 * must use an observed runtime probe before selecting a hardware code path. */
#define REAL_SIMD_NONE     0x00
#define REAL_SIMD_MMX      0x01
#define REAL_SIMD_SSE2     0x02
#define REAL_SIMD_AVX2     0x04
#define REAL_SIMD_AVX512   0x08
#define REAL_SIMD_NEON     0x10
#define REAL_SIMD_SVE      0x20
#define REAL_SIMD_RVV      0x40
#define REAL_SIMD_ALTIVEC  0x80

typedef enum {
  REAL_SYSCALL_LINUX_SYSCALL = 0,
  REAL_SYSCALL_LINUX_SVC     = 1,
  REAL_SYSCALL_DARWIN_SVC    = 2,
  REAL_SYSCALL_WASI          = 3,
  REAL_SYSCALL_UNKNOWN       = 255
} real_syscall_conv_t;

/* Relationship flags. SAME/ABI/ENDIAN/WORDSIZE describe catalog relations.
 * ISA_SUPER is nominal and does not prove OS execution support.
 * EMULATABLE is retained for ABI compatibility but is deliberately not inferred
 * by real_arch_compat(); emulator presence/execution requires runtime evidence. */
#define REAL_COMPAT_SAME        0x01
#define REAL_COMPAT_ABI         0x02
#define REAL_COMPAT_ENDIAN      0x04
#define REAL_COMPAT_WORDSIZE    0x08
#define REAL_COMPAT_ISA_SUPER   0x10
#define REAL_COMPAT_EMULATABLE  0x20

/* Reference profile. page_size/cache_line/simd are NOMINAL values, not runtime
 * capabilities. Field names remain unchanged to preserve the current C ABI. */
typedef struct {
  real_arch_t arch;
  const char *name;
  const char *canonical_uname;
  const char *termux_name;
  uint8_t word_bits;
  real_endian_t endian;
  uint32_t page_size;            /* nominal/reference only */
  uint32_t cache_line;           /* nominal/reference only */
  uint32_t simd;                 /* nominal/reference only */
  real_syscall_conv_t syscall_conv;
} real_arch_props_t;

/* Compile-time identity detection. */
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

/* Return nominal/reference properties for an architecture identifier. */
REAL_PURE
const real_arch_props_t *real_arch_props(real_arch_t arch);

REAL_CONST
real_arch_t real_arch_compile_time(void);

/* Detect runtime architecture identity via uname(2). This does not detect SIMD,
 * cache topology, page-size policy or emulator availability. */
REAL_WARN_UNUSED
real_arch_t real_arch_detect_runtime(void);

/* Return nominal catalog relationship flags. Never treat this function as an
 * execution/emulation receipt. */
REAL_PURE
uint32_t real_arch_compat(real_arch_t a, real_arch_t b);

REAL_PURE
const char *real_arch_name(real_arch_t arch);

REAL_PURE REAL_NONNULL_ALL
real_arch_t real_arch_from_name(const char *name);

/* Emit identity + nominal matrix JSON. */
REAL_COLD REAL_NONNULL_ALL
void real_arch_write_json(FILE *out);

REAL_COLD REAL_NONNULL_ALL
void real_arch_report(FILE *out);

#endif /* REAL_ARCH_H */
