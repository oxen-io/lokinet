#if __AVX2__
#include <immintrin.h>
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
  int minusmask = c;
  int plusmask  = -c;
  __m256i minusvec, plusvec, zerovec;

  minusmask >>= 31;
  plusmask >>= 31;
  minusvec = _mm256_set1_epi32(minusmask);
  plusvec  = _mm256_set1_epi32(plusmask);
  zerovec  = _mm256_set1_epi32(0);

  while(len >= 32)
  {
    __m256i xi = _mm256_loadu_si256((__m256i *)x);
    xi         = (xi & plusvec) | (_mm256_sub_epi8(zerovec, xi) & minusvec);
    _mm256_storeu_si256((__m256i *)z, xi);
    x += 32;
    z += 32;
    len -= 32;
  }

  for(i = 0; i < len; ++i)
    z[i] = mod3_product(x[i], c);
}

static void
vectormod3_minusproduct(small *z, int len, const small *x, const small *y,
                        const small c)
{
  int i;
  int minusmask = c;
  int plusmask  = -c;
  __m256i minusvec, plusvec, zerovec, twovec, fourvec;

  minusmask >>= 31;
  plusmask >>= 31;
  minusvec = _mm256_set1_epi32(minusmask);
  plusvec  = _mm256_set1_epi32(plusmask);
  zerovec  = _mm256_set1_epi32(0);
  twovec   = _mm256_set1_epi32(0x02020202);
  fourvec  = _mm256_set1_epi32(0x04040404);

  while(len >= 32)
  {
    __m256i xi = _mm256_loadu_si256((__m256i *)x);
    __m256i yi = _mm256_loadu_si256((__m256i *)y);
    __m256i r;
    yi = (yi & plusvec) | (_mm256_sub_epi8(zerovec, yi) & minusvec);
    xi = _mm256_sub_epi8(xi, yi);

    r = _mm256_add_epi8(xi, twovec);
    r &= fourvec;
    r  = _mm256_srli_epi32(r, 2);
    xi = _mm256_sub_epi8(xi, r);
    r  = _mm256_add_epi8(r, r);
    xi = _mm256_sub_epi8(xi, r);

    r = _mm256_sub_epi8(twovec, xi);
    r &= fourvec;
    r  = _mm256_srli_epi32(r, 2);
    xi = _mm256_add_epi8(xi, r);
    r  = _mm256_add_epi8(r, r);
    xi = _mm256_add_epi8(xi, r);

    _mm256_storeu_si256((__m256i *)z, xi);
    x += 32;
    y += 32;
    z += 32;
    len -= 32;
  }

  for(i = 0; i < len; ++i)
    z[i] = mod3_minusproduct(x[i], y[i], c);
}

static void
vectormod3_shift(small *z, int len)
{
  int i;
  while(len >= 33)
  {
    __m256i zi = _mm256_loadu_si256((__m256i *)(z + len - 33));
    _mm256_storeu_si256((__m256i *)(z + len - 32), zi);
    len -= 32;
  }
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
int
r3_recip(small *r, const small *s)
{
  const int loops = 2 * p + 1;
  int loop;
  small f[768];
  small g[769];
  small u[1536];
  small v[1537];
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

  for(i = 0; i <= loops; ++i)
    u[i] = 0;

  v[0] = 1;
  for(i = 1; i <= loops; ++i)
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

    if(loop >= loops)
      break;

    c = mod3_quotient(g[p], f[p]);

    vectormod3_minusproduct(g, 768, g, f, c);
    vectormod3_shift(g, 769);

#ifdef SIMPLER
    vectormod3_minusproduct(v, 1536, v, u, c);
    vectormod3_shift(v, 1537);
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
    swap(u, v, 1536 * sizeof(small), swapmask);
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
  for(i = p; i < 768; ++i)
    r[i] = 0;
  return smaller_mask(0, d);
}
#endif