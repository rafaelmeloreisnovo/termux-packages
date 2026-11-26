#include "raf_microasm_v2.h"
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_rev32(rafop_u32 a) {
  return ((a & 0x000000ffu) << 24u) | ((a & 0x0000ff00u) << 8u) | ((a & 0x00ff0000u) >> 8u) | ((a & 0xff000000u) >> 24u);
}
