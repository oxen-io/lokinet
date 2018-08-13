#include <string.h>
#include "modq.h"
#include "params.h"
#include "small.h"
#include "rq.h"
#include "crypto_kem.h"
#include "randombytes.h"
#include "crypto_stream_aes256ctr.h"

#if crypto_kem_PUBLICKEYBYTES != rq_encoderounded_len + 32
#error "crypto_kem_PUBLICKEYBYTES must match rq_encoderounded_len + 32"
#endif
#if crypto_kem_SECRETKEYBYTES != small_encode_len + crypto_kem_PUBLICKEYBYTES
#error "crypto_kem_SECRETKEYBYTES must match small_encode_len + crypto_kem_PUBLICKEYBYTES"
#endif

int crypto_kem_keypair(unsigned char *pk,unsigned char *sk)
{
  modq buf[768];
#define G buf
#define A buf
  small a[768];

  randombytes(pk,32);
  rq_fromseed(G,pk);

  small_random_weightw(a);

  rq_mult(A,G,a);

  rq_roundencode(pk + 32,A);

  small_encode(sk,a);
  memcpy(sk + small_encode_len,pk,crypto_kem_PUBLICKEYBYTES);

  return 0;
}
