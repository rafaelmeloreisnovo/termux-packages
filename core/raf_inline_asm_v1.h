#ifndef RAF_INLINE_ASM_V1_H
#define RAF_INLINE_ASM_V1_H

/* RAFAELIA authorial / freestanding pure 32-bit bit primitives.
 * ABI scope: ARMv7-A Thumb2/A32, AArch64; C fallback for host tests.
 * No libc, heap, syscalls, JNI, or runtime discovery.
 * This header is additive: no production bootstrap path is changed.
 */
typedef unsigned int raf_asm_u32;
typedef char raf_asm_u32_exactly_32_bits[(sizeof(raf_asm_u32) == 4) ? 1 : -1];

static __attribute__((always_inline)) inline raf_asm_u32
raf_asm_rev32(raf_asm_u32 v) {
#if defined(__aarch64__)
  raf_asm_u32 out;
  __asm__ volatile("rev %w0, %w1" : "=r"(out) : "r"(v));
  return out;
#elif defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
  raf_asm_u32 out;
  __asm__ volatile("rev %0, %1" : "=r"(out) : "r"(v));
  return out;
#else
  return ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8) |
         ((v & 0x00ff0000u) >> 8) | ((v & 0xff000000u) >> 24);
#endif
}

/* Zero convention: clz32(0) == ctz32(0) == 32, unlike UB-prone builtin_ctz(0). */
static __attribute__((always_inline)) inline raf_asm_u32
raf_asm_clz32(raf_asm_u32 v) {
#if defined(__aarch64__)
  raf_asm_u32 out;
  __asm__ volatile("clz %w0, %w1" : "=r"(out) : "r"(v));
  return out;
#elif defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
  raf_asm_u32 out;
  __asm__ volatile("clz %0, %1" : "=r"(out) : "r"(v));
  return out;
#else
  raf_asm_u32 n = 0u;
  for (raf_asm_u32 mask = 0x80000000u; mask && !(v & mask); mask >>= 1u) ++n;
  return n;
#endif
}

static __attribute__((always_inline)) inline raf_asm_u32
raf_asm_ctz32(raf_asm_u32 v) {
#if defined(__aarch64__)
  raf_asm_u32 out;
  __asm__ volatile("rbit %w0, %w1\n\tclz %w0, %w0" : "=r"(out) : "r"(v));
  return out;
#elif defined(__arm__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
  raf_asm_u32 out;
  __asm__ volatile("rbit %0, %1\n\tclz %0, %0" : "=r"(out) : "r"(v));
  return out;
#else
  raf_asm_u32 n = 0u;
  for (raf_asm_u32 mask = 1u; mask && !(v & mask); mask <<= 1u) ++n;
  return n;
#endif
}

#endif /* RAF_INLINE_ASM_V1_H */
