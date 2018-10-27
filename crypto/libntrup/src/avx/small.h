#ifndef small_h
#define small_h

#include <sodium/crypto_int8.h>

typedef crypto_int8 small;

#define small_encode crypto_kem_sntrup4591761_avx_small_encode
extern void
small_encode(unsigned char *, const small *);

#define small_decode crypto_kem_sntrup4591761_avx_small_decode
extern void
small_decode(small *, const unsigned char *);

#define small_random crypto_kem_sntrup4591761_avx_small_random
extern void
small_random(small *);

#define small_random_weightw crypto_kem_sntrup4591761_avx_small_random_weightw
extern void
small_random_weightw(small *);

#endif
