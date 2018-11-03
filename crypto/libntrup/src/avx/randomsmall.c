#include "params.h"
#include <sodium/randombytes.h>
#include <sodium/crypto_uint32.h>
#include "small.h"

void
small_random(small *g)
{
  crypto_uint32 r[p];
  int i;

  randombytes((unsigned char *)r, sizeof r);
  for(i = 0; i < p; ++i)
    g[i] = (small)(((r[i] & 1073741823) * 3) >> 30) - 1;
  /* bias is miniscule */
  for(i = p; i < 768; ++i)
    g[i] = 0;
}
