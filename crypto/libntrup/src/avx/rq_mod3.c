#if __AVX2__
#include <immintrin.h>
#include <smmintrin.h>
#include "mod3.h"
#include "rq.h"

#define v3 _mm256_set1_epi16(3)
#define v7 _mm256_set1_epi16(7)
#define v2296_16 _mm256_set1_epi16(2296)
#define v4591_16 _mm256_set1_epi16(4591)
#define v10923_16 _mm256_set1_epi16(10923)

// 32-bit hosts only
#ifndef __amd64__
#define _mm_extract_epi64(X, N) \
  (__extension__({              \
    __v2di __a = (__v2di)(X);   \
    __a[N];                     \
  }))
#endif

static inline __m256i
squeeze(__m256i x)
{
  __m256i q = _mm256_mulhrs_epi16(x, v7);
  q         = _mm256_mullo_epi16(q, v4591_16);
  return _mm256_sub_epi16(x, q);
}

static inline __m256i
freeze(__m256i x)
{
  __m256i mask, x2296, x4591;
  x4591 = _mm256_add_epi16(x, v4591_16);
  mask  = _mm256_srai_epi16(x, 15);
  x     = _mm256_blendv_epi8(x, x4591, mask);
  x2296 = _mm256_sub_epi16(x, v2296_16);
  mask  = _mm256_srai_epi16(x2296, 15);
  x4591 = _mm256_sub_epi16(x, v4591_16);
  x     = _mm256_blendv_epi8(x4591, x, mask);
  return x;
}

void
rq_mod3(small *g, const modq *f)
{
  int i;

  for(i = 0; i < 768; i += 16)
  {
    __m256i x = _mm256_loadu_si256((__m256i *)&f[i]);
    __m256i q;
    x = _mm256_mullo_epi16(x, v3);
    x = squeeze(x);
    x = freeze(x);
    q = _mm256_mulhrs_epi16(x, v10923_16);
    x = _mm256_sub_epi16(x, q);
    q = _mm256_add_epi16(q, q);
    x = _mm256_sub_epi16(x, q); /* g0 g1 ... g15 */
    x = _mm256_packs_epi16(x,
                           x); /* g0 ... g7 g0 ... g7 g8 ... g15 g8 ... g15 */
    0 [(long long *)&g[i]] =
        _mm_extract_epi64(_mm256_extracti128_si256(x, 0), 0);
    1 [(long long *)&g[i]] =
        _mm_extract_epi64(_mm256_extracti128_si256(x, 1), 0);
  }
}
#endif