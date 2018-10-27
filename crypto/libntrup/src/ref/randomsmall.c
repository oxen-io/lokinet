#include "params.h"
#include <sodium/randombytes.h>
#include <sodium/crypto_uint32.h>
#include "small.h"

void
small_random(small *g)
{
  int i;

  for(i = 0; i < p; ++i)
  {
    crypto_uint32 r = small_random32();
    g[i]            = (small)(((1073741823 & r) * 3) >> 30) - 1;
  }
}
