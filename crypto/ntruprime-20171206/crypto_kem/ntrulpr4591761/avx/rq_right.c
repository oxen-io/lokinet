#include "rq.h"
#include "params.h"

void rq_rightsubbit(unsigned char *r,const unsigned char *c,const modq *ab)
{
  modq t[256];
  int i;

  for (i = 0;i < 128;++i) {
    crypto_uint32 x = c[i];
    t[2*i] = (x & 15) * 287 - 2007;
    t[2*i+1] = (x >> 4) * 287 - 2007;
  }

  for (i = 0;i < 256;++i)
    t[i] = -(modq_freeze(t[i] - ab[i] + 4*w+1) >> 14);

  for (i = 0;i < 32;++i) r[i] = 0;
  for (i = 0;i < 256;++i)
    r[i / 8] |= (t[i] << (i & 7));
}
