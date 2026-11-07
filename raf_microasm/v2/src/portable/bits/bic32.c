#include "raf_microasm_v2.h"
/* One file = one callable primitive; compiled with its manifest flags. */
rafop_u32 rafop_bic32(rafop_u32 a, rafop_u32 b) {
  return a & ~b;
}
