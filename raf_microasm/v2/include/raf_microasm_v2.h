#ifndef RAF_MICROASM_V2_H
#define RAF_MICROASM_V2_H
/* Independent per-function ABI. Pure/freestanding; no runtime dependency. */
typedef unsigned int rafop_u32;
typedef char rafop_assert_u32[(sizeof(rafop_u32)==4) ? 1 : -1];
#if defined(__GNUC__) || defined(__clang__)
#define RAFOP_API __attribute__((visibility("default"),used,noinline))
#else
#define RAFOP_API
#endif
RAFOP_API rafop_u32 rafop_rev32(rafop_u32 a);
RAFOP_API rafop_u32 rafop_rev16(rafop_u32 a);
RAFOP_API rafop_u32 rafop_rbit32(rafop_u32 a);
RAFOP_API rafop_u32 rafop_clz32(rafop_u32 a);
RAFOP_API rafop_u32 rafop_ctz32(rafop_u32 a);
RAFOP_API rafop_u32 rafop_ror32(rafop_u32 a, rafop_u32 b);
RAFOP_API rafop_u32 rafop_lsl32(rafop_u32 a, rafop_u32 b);
RAFOP_API rafop_u32 rafop_lsr32(rafop_u32 a, rafop_u32 b);
RAFOP_API rafop_u32 rafop_xor32(rafop_u32 a, rafop_u32 b);
RAFOP_API rafop_u32 rafop_and32(rafop_u32 a, rafop_u32 b);
RAFOP_API rafop_u32 rafop_orr32(rafop_u32 a, rafop_u32 b);
RAFOP_API rafop_u32 rafop_bic32(rafop_u32 a, rafop_u32 b);
#endif /* RAF_MICROASM_V2_H */
