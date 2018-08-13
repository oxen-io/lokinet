#ifndef mod3_h
#define mod3_h

#include "small.h"
#include "crypto_int32.h"

/* -1 if x is nonzero, 0 otherwise */
static inline int mod3_nonzero_mask(small x)
{
  return -x*x;
}

/* input between -100000 and 100000 */
/* output between -1 and 1 */
static inline small mod3_freeze(crypto_int32 a)
{
  a -= 3 * ((10923 * a) >> 15);
  a -= 3 * ((89478485 * a + 134217728) >> 28);
  return a;
}

static inline small mod3_minusproduct(small a,small b,small c)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  crypto_int32 C = c;
  return mod3_freeze(A - B * C);
}

static inline small mod3_plusproduct(small a,small b,small c)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  crypto_int32 C = c;
  return mod3_freeze(A + B * C);
}

static inline small mod3_product(small a,small b)
{
  return a * b;
}

static inline small mod3_sum(small a,small b)
{
  crypto_int32 A = a;
  crypto_int32 B = b;
  return mod3_freeze(A + B);
}

static inline small mod3_reciprocal(small a1)
{
  return a1;
}

static inline small mod3_quotient(small num,small den)
{
  return mod3_product(num,mod3_reciprocal(den));
}

#endif
