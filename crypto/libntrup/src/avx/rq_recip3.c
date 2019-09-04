#if __AVX2__
#include <immintrin.h>
#include "params.h"
#include "swap.h"
#include "rq.h"

#define v7 _mm256_set1_epi16(7)
#define v1827_16 _mm256_set1_epi16(1827)
#define v4591_16 _mm256_set1_epi16(4591)
#define v29234_16 _mm256_set1_epi16(29234)

/* caller must ensure that x-y does not overflow */
static int
smaller_mask(int x, int y)
{
  return (x - y) >> 31;
}

static inline __m256i
product(__m256i x, __m256i y)
{
  __m256i lo, hi, r0, r1, t0, t1, t, s0, s1;

  lo = _mm256_mullo_epi16(x, y);
  hi = _mm256_mulhi_epi16(x, y);
  r0 = _mm256_unpacklo_epi16(lo, hi);
  r1 = _mm256_unpackhi_epi16(lo, hi);

  t0 = _mm256_srai_epi32(r0, 16);
  t1 = _mm256_srai_epi32(r1, 16);
  t  = _mm256_packs_epi32(t0, t1);
  t  = _mm256_mulhrs_epi16(t, v29234_16);
  lo = _mm256_mullo_epi16(t, v4591_16);
  hi = _mm256_mulhi_epi16(t, v4591_16);
  s0 = _mm256_unpacklo_epi16(lo, hi);
  s1 = _mm256_unpackhi_epi16(lo, hi);
  s0 = _mm256_slli_epi32(s0, 4);
  s1 = _mm256_slli_epi32(s1, 4);
  r0 = _mm256_sub_epi32(r0, s0);
  r1 = _mm256_sub_epi32(r1, s1);

  t0 = _mm256_srai_epi32(r0, 8);
  t1 = _mm256_srai_epi32(r1, 8);
  t  = _mm256_packs_epi32(t0, t1);
  t  = _mm256_mulhrs_epi16(t, v1827_16);
  lo = _mm256_mullo_epi16(t, v4591_16);
  hi = _mm256_mulhi_epi16(t, v4591_16);
  s0 = _mm256_unpacklo_epi16(lo, hi);
  s1 = _mm256_unpackhi_epi16(lo, hi);
  r0 = _mm256_sub_epi32(r0, s0);
  r1 = _mm256_sub_epi32(r1, s1);

  x = _mm256_packs_epi32(r0, r1);
  return x;
}

static inline __m256i
minusproduct(__m256i x, __m256i y, __m256i z)
{
  __m256i t;

  x = _mm256_sub_epi16(x, product(y, z));
  t = _mm256_mulhrs_epi16(x, v7);
  t = _mm256_mullo_epi16(t, v4591_16);
  x = _mm256_sub_epi16(x, t);
  return x;
}

static void
vectormodq_product(modq *z, int len, const modq *x, const modq c)
{
  __m256i cvec = _mm256_set1_epi16(c);
  while(len >= 16)
  {
    __m256i xi = _mm256_loadu_si256((__m256i *)x);
    xi         = product(xi, cvec);
    _mm256_storeu_si256((__m256i *)z, xi);
    x += 16;
    z += 16;
    len -= 16;
  }
  while(len > 0)
  {
    *z = modq_product(*x, c);
    ++x;
    ++z;
    --len;
  }
}

static void
vectormodq_minusproduct(modq *z, int len, const modq *x, const modq *y,
                        const modq c)
{
  __m256i cvec = _mm256_set1_epi16(c);
  while(len >= 16)
  {
    __m256i xi = _mm256_loadu_si256((__m256i *)x);
    __m256i yi = _mm256_loadu_si256((__m256i *)y);
    xi         = minusproduct(xi, yi, cvec);
    _mm256_storeu_si256((__m256i *)z, xi);
    x += 16;
    y += 16;
    z += 16;
    len -= 16;
  }
  while(len > 0)
  {
    *z = modq_minusproduct(*x, *y, c);
    ++x;
    ++y;
    ++z;
    --len;
  }
}

static void
vectormodq_shift(modq *z, int len)
{
  int i;
  while(len >= 17)
  {
    __m256i zi = _mm256_loadu_si256((__m256i *)(z + len - 17));
    _mm256_storeu_si256((__m256i *)(z + len - 16), zi);
    len -= 16;
  }
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
int
rq_recip3(modq *r, const small *s)
{
  const int loops = 2 * p + 1;
  int loop;
  modq f[768];
  modq g[769];
  modq u[1536];
  modq v[1537];
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

    c = modq_quotient(g[p], f[p]);

    vectormodq_minusproduct(g, 768, g, f, c);
    vectormodq_shift(g, 769);

#ifdef SIMPLER
    vectormodq_minusproduct(v, 1536, v, u, c);
    vectormodq_shift(v, 1537);
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
    swap(f, g, 768 * sizeof(modq), swapmask);

#ifdef SIMPLER
    swap(u, v, 1536 * sizeof(modq), swapmask);
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
  for(i = 0; i < p; ++i)
    r[i] = modq_freeze(r[i]);
  for(i = p; i < 768; ++i)
    r[i] = 0;
  return smaller_mask(0, d);
}
#endif