#include "raf_microasm_v2.h"
#if !defined(__aarch64__)
#error "aarch64 only"
#endif
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_bic32(rafop_u32 a, rafop_u32 b) {
  rafop_u32 out;
  __asm__ volatile("bic %w0, %w1, %w2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}
