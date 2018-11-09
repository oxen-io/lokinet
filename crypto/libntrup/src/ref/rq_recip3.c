#include "params.h"
#include "swap.h"
#include "rq.h"

/* caller must ensure that x-y does not overflow */
static int
smaller_mask(int x, int y)
{
  return (x - y) >> 31;
}

static void
vectormodq_product(modq *z, int len, const modq *x, const modq c)
{
  int i;
  for(i = 0; i < len; ++i)
    z[i] = modq_product(x[i], c);
}

static void
vectormodq_minusproduct(modq *z, int len, const modq *x, const modq *y,
                        const modq c)
{
  int i;
  for(i = 0; i < len; ++i)
    z[i] = modq_minusproduct(x[i], y[i], c);
}

static void
vectormodq_shift(modq *z, int len)
{
  int i;
  for(i = len - 1; i > 0; --i)
    z[i] = z[i - 1];
  z[0] = 0;
}

/*
r = (3s)^(-1) mod m, returning 0, if s is invertible mod m
or returning -1 if s is not invertible mod m
r,s are polys of degree <p
m is x^p-x-1
*/
#define LOOPS (2 * p + 1)

int
rq_recip3(modq *r, const small *s)
{
  int loop;
  modq f[p + 1];
  modq g[p + 1];

  modq u[LOOPS + 1];
  modq v[LOOPS + 1];

  modq c;
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
    g[i] = 3 * s[i];
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

    c = modq_quotient(g[p], f[p]);

    vectormodq_minusproduct(g, p + 1, g, f, c);
    vectormodq_shift(g, p + 1);

#ifdef SIMPLER
    vectormodq_minusproduct(v, loops + 1, v, u, c);
    vectormodq_shift(v, loops + 1);
#else
    if(loop < p)
    {
      vectormodq_minusproduct(v, loop + 1, v, u, c);
      vectormodq_shift(v, loop + 2);
    }
    else
    {
      vectormodq_minusproduct(v + loop - p, p + 1, v + loop - p, u + loop - p,
                              c);
      vectormodq_shift(v + loop - p, p + 2);
    }
#endif

    e -= 1;

    ++loop;

    swapmask = smaller_mask(e, d) & modq_nonzero_mask(g[p]);
    swap(&e, &d, sizeof e, swapmask);
    swap(f, g, (p + 1) * sizeof(modq), swapmask);

#ifdef SIMPLER
    swap(u, v, (loops + 1) * sizeof(modq), swapmask);
#else
    if(loop < p)
    {
      swap(u, v, (loop + 1) * sizeof(modq), swapmask);
    }
    else
    {
      swap(u + loop - p, v + loop - p, (p + 1) * sizeof(modq), swapmask);
    }
#endif
  }

  c = modq_reciprocal(f[p]);
  vectormodq_product(r, p, u + p, c);
  return smaller_mask(0, d);
}
