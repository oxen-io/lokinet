#include "rq.h"

void rq_top(unsigned char *c,const modq *f,const unsigned char *r)
{
  modq T[256];
  int i;

  for (i = 0;i < 256;++i) {
    modq x = f[i];
    x = modq_sum(x,2295 * (1 & (r[i / 8] >> (i & 7))));
    x = ((x + 2156) * 114 + 16384) >> 15;
    T[i] = x; /* between 0 and 15 */
  }

  for (i = 0;i < 128;++i)
    *c++ = T[2*i] + (T[2*i + 1] << 4);
}
