#include "raf_microasm_v2.h"
#if !defined(__arm__) || !defined(__ARM_ARCH) || __ARM_ARCH < 7
#error "ARMv7+ only"
#endif
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_bic32(rafop_u32 a, rafop_u32 b) {
  rafop_u32 out;
  __asm__ volatile("bic %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}
