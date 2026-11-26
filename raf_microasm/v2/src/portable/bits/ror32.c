#include "raf_microasm_v2.h"
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_ror32(rafop_u32 a, rafop_u32 b) {
  b &= 31u; return (a >> b) | (a << ((32u-b)&31u));
}
