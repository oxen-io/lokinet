#ifndef r3_h
#define r3_h

#include "small.h"

#define r3_mult crypto_kem_sntrup4591761_avx_r3_mult
extern void
r3_mult(small *, const small *, const small *);

#define r3_recip crypto_kem_sntrup4591761_avx_r3_recip
extern int
r3_recip(small *, const small *);

#define r3_weightw_mask crypto_kem_sntrup4591761_avx_r3_weightw_mask
extern int
r3_weightw_mask(const small *);

#endif
