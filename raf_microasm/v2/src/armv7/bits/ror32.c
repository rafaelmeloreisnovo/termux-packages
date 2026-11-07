#include "raf_microasm_v2.h"
#if !defined(__arm__) || !defined(__ARM_ARCH) || __ARM_ARCH < 7
#error "ARMv7+ only"
#endif
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_ror32(rafop_u32 a, rafop_u32 b) {
  b &= 31u;
  rafop_u32 out;
  __asm__ volatile("ror %0, %1, %2" : "=r"(out) : "r"(a), "r"(b));
  return out;
}
