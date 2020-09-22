#include <string.h>
#include "modq.h"
#include "params.h"
#include "r3.h"
#include "small.h"
#include "rq.h"
#include <sodium/crypto_kem.h>

#if crypto_kem_PUBLICKEYBYTES != rq_encode_len
#error "crypto_kem_PUBLICKEYBYTES must match rq_encode_len"
#endif
#if crypto_kem_SECRETKEYBYTES != rq_encode_len + 2 * small_encode_len
#error \
    "crypto_kem_SECRETKEYBYTES must match rq_encode_len + 2 * small_encode_len"
#endif

#ifndef __AVX2__
#error "This file requires compilation with AVX2 support"
#endif

int
crypto_kem_keypair_avx2(unsigned char *pk, unsigned char *sk)
{
  small g[768];
  small grecip[768];
  small f[768];
  modq f3recip[768];
  modq h[768];

  do
    small_random(g);
  while(r3_recip(grecip, g) != 0);

  small_random_weightw(f);
  rq_recip3(f3recip, f);

  rq_mult(h, f3recip, g);

  rq_encode(pk, h);
  small_encode(sk, f);
  small_encode(sk + small_encode_len, grecip);
  memcpy(sk + 2 * small_encode_len, pk, rq_encode_len);

  return 0;
}
