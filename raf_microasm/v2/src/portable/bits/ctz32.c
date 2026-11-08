#include "raf_microasm_v2.h"
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_ctz32(rafop_u32 a) {
  if (!a) return 32u;
  rafop_u32 n = 0u;
  for (rafop_u32 bit = 1u; (a & bit) == 0u; bit <<= 1u) ++n;
  return n;
}
