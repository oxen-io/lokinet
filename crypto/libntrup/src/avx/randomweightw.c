#include "params.h"
#include <sodium/randombytes.h>
#include "int32_sort.h"
#include "small.h"

void
small_random_weightw(small *f)
{
  crypto_int32 r[p];
  int i;

  randombytes((unsigned char *)r, sizeof r);
  for(i = 0; i < w; ++i)
    r[i] &= -2;
  for(i = w; i < p; ++i)
    r[i] = (r[i] & -3) | 1;
  int32_sort(r, p);
  for(i = 0; i < p; ++i)
    f[i] = ((small)(r[i] & 3)) - 1;
  for(i = p; i < 768; ++i)
    f[i] = 0;
}
