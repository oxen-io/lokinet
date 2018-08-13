#ifdef KAT
#include <stdio.h>
#endif

#include "params.h"
#include "small.h"
#include "rq.h"
#include "hide.h"
#include "crypto_kem.h"

static int verify(const unsigned char *x,const unsigned char *y)
{
  unsigned int differentbits = 0;
  int i;
  for (i = 0;i < crypto_kem_CIPHERTEXTBYTES;++i)
    differentbits |= x[i] ^ y[i];
  return (1 & ((differentbits - 1) >> 8)) - 1;
}

int crypto_kem_dec(
  unsigned char *k,
  const unsigned char *cstr,
  const unsigned char *sk
)
{
  modq buf[768];
#define B buf
#define aB buf
  small a[768];
  unsigned char r[32];
  unsigned char checkcstr[crypto_kem_CIPHERTEXTBYTES];
  unsigned char maybek[32];
  int i;
  int result;

  small_decode(a,sk); sk += small_encode_len;
  rq_decoderounded(B,cstr + 32);
  rq_mult(aB,B,a);

  rq_rightsubbit(r,cstr + 32 + rq_encoderounded_len,aB);

#ifdef KAT
  {
    int j;
    printf("decrypt r: ");
    for (j = 0;j < 32;++j)
      printf("%02x",255 & (int) r[j]);
    printf("\n");
  }
#endif

  hide(checkcstr,maybek,sk,r);
  result = verify(cstr,checkcstr);

  for (i = 0;i < 32;++i) k[i] = maybek[i] & ~result;
  return result;
}
