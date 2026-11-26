#include "raf_microasm_v2.h"
#ifndef RAFOP_NEGATIVE
static rafop_u32 ref_rbit32(rafop_u32 a) { rafop_u32 x=0u; for(rafop_u32 i=0u;i<32u;++i) x=(x<<1u)|((a>>i)&1u);return x; }
static rafop_u32 ref_clz32(rafop_u32 a) { rafop_u32 n=0u;for(rafop_u32 i=0u;i<32u;++i){if(a & (0x80000000u>>i))break;++n;}return n; }
static rafop_u32 ref_ctz32(rafop_u32 a) { rafop_u32 n=0u;for(rafop_u32 i=0u;i<32u;++i){if(a & (1u<<i))break;++n;}return n; }
static rafop_u32 ref_rev32(rafop_u32 a) {return ((a&0xffu)<<24u)|((a&0xff00u)<<8u)|((a>>8u)&0xff00u)|(a>>24u);}
static int check(rafop_u32 a,rafop_u32 b){
 if(rafop_rev32(a)!=ref_rev32(a))return 1;
 if(rafop_rev16(a)!=(((a&0x00ff00ffu)<<8u)|((a&0xff00ff00u)>>8u)))return 2;
 if(rafop_rbit32(a)!=ref_rbit32(a))return 3;
 if(rafop_clz32(a)!=ref_clz32(a))return 4;
 if(rafop_ctz32(a)!=ref_ctz32(a))return 5;
 b &= 31u;
 if(rafop_ror32(a,b)!=((a>>b)|(a<<((32u-b)&31u))))return 6;
 if(rafop_lsl32(a,b)!=(a<<b))return 7;
 if(rafop_lsr32(a,b)!=(a>>b))return 8;
 if(rafop_xor32(a,b)!=(a^b))return 9;
 if(rafop_and32(a,b)!=(a&b))return 10;
 if(rafop_orr32(a,b)!=(a|b))return 11;
 if(rafop_bic32(a,b)!=(a&~b))return 12;
 return 0;
}
#endif /* !RAFOP_NEGATIVE */
int main(void){
#ifdef RAFOP_NEGATIVE
 return rafop_rev32(0x12345678u)==0x78563413u ? 0 : 1;
#else
 const rafop_u32 boundary[]={0u,1u,0x80000000u,0xffffffffu,0x12345678u,0x01000000u,0x80808080u};
 for(rafop_u32 i=0u;i<7u;++i)for(rafop_u32 s=0u;s<32u;++s)if(check(boundary[i],s))return 100+(int)i;
 rafop_u32 state=0x12345678u;
 for(rafop_u32 i=0u;i<1000u;++i){state=state*1664525u+1013904223u;if(check(state,i))return 200+(int)(i%32u);}
 return 0;
#endif
}
