#include "raf_microasm_v2.h"
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_rev16(rafop_u32 a) {
  return ((a & 0x00ff00ffu) << 8u) | ((a & 0xff00ff00u) >> 8u);
}
