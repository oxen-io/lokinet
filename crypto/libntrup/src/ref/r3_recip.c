#include "params.h"
#include "mod3.h"
#include "swap.h"
#include "r3.h"

/* caller must ensure that x-y does not overflow */
static int
smaller_mask(int x, int y)
{
  return (x - y) >> 31;
}

static void
vectormod3_product(small *z, int len, const small *x, const small c)
{
  int i;
  for(i = 0; i < len; ++i)
    z[i] = mod3_product(x[i], c);
}

static void
vectormod3_minusproduct(small *z, int len, const small *x, const small *y,
                        const small c)
{
  int i;
  for(i = 0; i < len; ++i)
    z[i] = mod3_minusproduct(x[i], y[i], c);
}

static void
vectormod3_shift(small *z, int len)
{
  int i;
  for(i = len - 1; i > 0; --i)
    z[i] = z[i - 1];
  z[0] = 0;
}

/*
r = s^(-1) mod m, returning 0, if s is invertible mod m
or returning -1 if s is not invertible mod m
r,s are polys of degree <p
m is x^p-x-1
*/
#define LOOPS (2 * p + 1)
int
r3_recip(small *r, const small *s)
{
  int loop;
  small f[p + 1];
  small g[p + 1];

  small u[LOOPS + 1];
  small v[LOOPS + 1];

  small c;
  int i;
  int d = p;
  int e = p;
  int swapmask;

  for(i = 2; i < p; ++i)
    f[i] = 0;
  f[0] = -1;
  f[1] = -1;
  f[p] = 1;
  /* generalization: can initialize f to any polynomial m */
  /* requirements: m has degree exactly p, nonzero constant coefficient */

  for(i = 0; i < p; ++i)
    g[i] = s[i];
  g[p] = 0;

  for(i = 0; i <= LOOPS; ++i)
    u[i] = 0;

  v[0] = 1;
  for(i = 1; i <= LOOPS; ++i)
    v[i] = 0;

  loop = 0;
  for(;;)
  {
    /* e == -1 or d + e + loop <= 2*p */

    /* f has degree p: i.e., f[p]!=0 */
    /* f[i]==0 for i < p-d */

    /* g has degree <=p (so it fits in p+1 coefficients) */
    /* g[i]==0 for i < p-e */

    /* u has degree <=loop (so it fits in loop+1 coefficients) */
    /* u[i]==0 for i < p-d */
    /* if invertible: u[i]==0 for i < loop-p (so can look at just p+1
     * coefficients) */

    /* v has degree <=loop (so it fits in loop+1 coefficients) */
    /* v[i]==0 for i < p-e */
    /* v[i]==0 for i < loop-p (so can look at just p+1 coefficients) */

    if(loop >= LOOPS)
      break;

    c = mod3_quotient(g[p], f[p]);

    vectormod3_minusproduct(g, p + 1, g, f, c);
    vectormod3_shift(g, p + 1);

#ifdef SIMPLER
    vectormod3_minusproduct(v, loops + 1, v, u, c);
    vectormod3_shift(v, loops + 1);
#else
    if(loop < p)
    {
      vectormod3_minusproduct(v, loop + 1, v, u, c);
      vectormod3_shift(v, loop + 2);
    }
    else
    {
      vectormod3_minusproduct(v + loop - p, p + 1, v + loop - p, u + loop - p,
                              c);
      vectormod3_shift(v + loop - p, p + 2);
    }
#endif

    e -= 1;

    ++loop;

    swapmask = smaller_mask(e, d) & mod3_nonzero_mask(g[p]);
    swap(&e, &d, sizeof e, swapmask);
    swap(f, g, (p + 1) * sizeof(small), swapmask);

#ifdef SIMPLER
    swap(u, v, (loops + 1) * sizeof(small), swapmask);
#else
    if(loop < p)
    {
      swap(u, v, (loop + 1) * sizeof(small), swapmask);
    }
    else
    {
      swap(u + loop - p, v + loop - p, (p + 1) * sizeof(small), swapmask);
    }
#endif
  }

  c = mod3_reciprocal(f[p]);
  vectormod3_product(r, p, u + p, c);
  return smaller_mask(0, d);
}
