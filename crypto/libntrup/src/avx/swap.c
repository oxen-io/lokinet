#if __AVX2__
#include <immintrin.h>
#include "swap.h"

void
swap(void *x, void *y, int bytes, int mask)
{
  char c          = mask;
  __m256i maskvec = _mm256_set1_epi32(mask);

  while(bytes >= 32)
  {
    __m256i xi    = _mm256_loadu_si256(x);
    __m256i yi    = _mm256_loadu_si256(y);
    __m256i xinew = _mm256_blendv_epi8(xi, yi, maskvec);
    __m256i yinew = _mm256_blendv_epi8(yi, xi, maskvec);
    _mm256_storeu_si256(x, xinew);
    _mm256_storeu_si256(y, yinew);
    x = 32 + (char *)x;
    y = 32 + (char *)y;
    bytes -= 32;
  }
  while(bytes > 0)
  {
    char xi = *(char *)x;
    char yi = *(char *)y;
    char t  = c & (xi ^ yi);
    xi ^= t;
    yi ^= t;
    *(char *)x = xi;
    *(char *)y = yi;
    ++x;
    ++y;
    --bytes;
  }
}
#endif