#include "raf_inline_asm_v1.h"

/* No I/O, no ambient state; 0=vector pass, nonzero=first failed vector. */
int raf_inline_asm_v1_selftest(void) {
  static const raf_asm_u32 input[] = {
      0x00000000u, 0x00000001u, 0x80000000u,
      0x12345678u, 0xffffffffu, 0x01000000u};
  static const raf_asm_u32 rev[] = {
      0x00000000u, 0x01000000u, 0x00000080u,
      0x78563412u, 0xffffffffu, 0x00000001u};
  static const raf_asm_u32 clz[] = {32u, 31u, 0u, 3u, 0u, 7u};
  static const raf_asm_u32 ctz[] = {32u, 0u, 31u, 3u, 0u, 24u};
  for (raf_asm_u32 i = 0u; i < 6u; ++i) {
    if (raf_asm_rev32(input[i]) != rev[i]) return (int)(1u + i);
    if (raf_asm_clz32(input[i]) != clz[i]) return (int)(11u + i);
    if (raf_asm_ctz32(input[i]) != ctz[i]) return (int)(21u + i);
  }
  return 0;
}
#ifdef RAF_ASM_HOST_MAIN
int main(void) { return raf_inline_asm_v1_selftest(); }
#endif
