#if __AVX2__
#include <immintrin.h>
#include "params.h"
#include "rq.h"

#define v3_16 _mm256_set1_epi16(3)
#define v10923_16 _mm256_set1_epi16(10923)

void
rq_round3(modq *h, const modq *f)
{
  int i;

  for(i = 0; i < 768; i += 16)
  {
    __m256i x = _mm256_loadu_si256((__m256i *)&f[i]);
    __m256i x2;
    x  = _mm256_mulhrs_epi16(x, v10923_16);
    x2 = _mm256_add_epi16(x, x);
    x  = _mm256_add_epi16(x, x2);
    _mm256_storeu_si256((__m256i *)&h[i], x);
  }
}
#endif