#ifndef modq_h
#define modq_h

#include "crypto_int16.h"
#include "crypto_int32.h"
#include "crypto_uint16.h"
#include "crypto_uint32.h"

typedef crypto_int16 modq;

/* input between -9000000 and 9000000 */
/* output between -2295 and 2295 */
static inline modq modq_freeze(crypto_int32 a)
{
  a -= 4591 * ((228 * a) >> 20);
  a -= 4591 * ((58470 * a + 134217728) >> 28);
  return a;
}

/* input between 0 and 4294967295 */
/* output = (input % 4591) - 2295 */
static inline modq modq_fromuint32(crypto_uint32 a)
{
  crypto_int32 r;
  r = (a & 524287) + (a >> 19) * 914; /* <= 8010861 */
  return modq_freeze(r - 2295);
}

static inline modq modq_sum(modq a,modq b)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  return modq_freeze(A + B);
}

#endif
