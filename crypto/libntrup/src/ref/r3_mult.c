#include "params.h"
#include "mod3.h"
#include "r3.h"

void
r3_mult(small *h, const small *f, const small *g)
{
  small fg[p + p - 1];
  small result;
  int i, j;

  for(i = 0; i < p; ++i)
  {
    result = 0;
    for(j = 0; j <= i; ++j)
      result = mod3_plusproduct(result, f[j], g[i - j]);
    fg[i] = result;
  }
  for(i = p; i < p + p - 1; ++i)
  {
    result = 0;
    for(j = i - p + 1; j < p; ++j)
      result = mod3_plusproduct(result, f[j], g[i - j]);
    fg[i] = result;
  }

  for(i = p + p - 2; i >= p; --i)
  {
    fg[i - p]     = mod3_sum(fg[i - p], fg[i]);
    fg[i - p + 1] = mod3_sum(fg[i - p + 1], fg[i]);
  }

  for(i = 0; i < p; ++i)
    h[i] = fg[i];
}
