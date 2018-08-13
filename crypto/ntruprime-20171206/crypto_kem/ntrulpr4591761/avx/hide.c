#include <string.h>
#include "crypto_hash_sha512.h"
#include "crypto_kem.h"
#include "params.h"
#include "rq.h"
#include "hide.h"

#if crypto_kem_CIPHERTEXTBYTES != rq_encoderounded_len + 32 + 128
#error "crypto_kem_CIPHERTEXTBYTES must match rq_encoderounded_len + 32 + 128"
#endif

void hide(unsigned char *cstr,unsigned char *k,const unsigned char *pk,const unsigned char *r)
{
  modq buf[768];
#define G buf
#define A buf
#define B buf
#define C buf
  unsigned char k12[64];
  unsigned char k34[64];
  small b[768];

  crypto_hash_sha512(k12,r,32);
  small_seeded_weightw(b,k12);

  crypto_hash_sha512(k34,k12 + 32,32);
  memcpy(cstr,k34,32); cstr += 32;
  memcpy(k,k34 + 32,32);

  rq_fromseed(G,pk);
  rq_mult(B,G,b);
  /* XXX: cache transform of b for next mult */
  /* XXX: cache transform of G inside sk */
  /* XXX: cache transform of G when pk is otherwise reused */
  rq_roundencode(cstr,B); cstr += rq_encoderounded_len;

  rq_decoderounded(A,pk + 32);
  rq_mult(C,A,b);
  rq_top(cstr,C,r);
}
