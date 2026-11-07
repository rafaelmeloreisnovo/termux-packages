#include "raf_microasm_v2.h"
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_rbit32(rafop_u32 a) {
  a = ((a & 0x55555555u) << 1u) | ((a >> 1u) & 0x55555555u);
  a = ((a & 0x33333333u) << 2u) | ((a >> 2u) & 0x33333333u);
  a = ((a & 0x0f0f0f0fu) << 4u) | ((a >> 4u) & 0x0f0f0f0fu);
  a = ((a & 0x00ff00ffu) << 8u) | ((a >> 8u) & 0x00ff00ffu);
  return (a << 16u) | (a >> 16u);
}
