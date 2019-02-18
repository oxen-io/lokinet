#include <sodium/randombytes.h>
#include "small.h"

#ifdef KAT
/* NIST KAT generator fails to provide chunk-independence */
static unsigned char x[4 * 761];
static long long pos = 4 * 761;
#endif

crypto_int32
small_random32(void)
{
#ifdef KAT
  if(pos == 4 * 761)
  {
    randombytes(x, sizeof x);
    pos = 0;
  }
  pos += 4;
  return x[pos - 4] + (x[pos - 3] << 8) + (x[pos - 2] << 16)
      + (x[pos - 1] << 24);
#else
  unsigned char x[4];
  randombytes(x, 4);
  uint32_t x4 = x[3] << 24;
  return x[0] + (x[1] << 8) + (x[2] << 16) + x4;
#endif
}
