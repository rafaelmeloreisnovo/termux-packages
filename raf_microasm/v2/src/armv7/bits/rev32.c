#include "raf_microasm_v2.h"
#if !defined(__arm__) || !defined(__ARM_ARCH) || __ARM_ARCH < 7
#error "ARMv7+ only"
#endif
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_rev32(rafop_u32 a) {
  rafop_u32 out;
  __asm__ volatile("rev %0, %1" : "=r"(out) : "r"(a));
  return out;
}
