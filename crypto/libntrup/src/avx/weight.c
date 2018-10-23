#if __AVX2__
#include <immintrin.h>
#include "params.h"
#include "r3.h"
#include <sodium/crypto_uint16.h>
#include <sodium/crypto_int32.h>

int
r3_weightw_mask(const small *r)
{
  int weight;
  int i;
  __m256i tally = _mm256_set1_epi32(0);

  for(i = 0; i < 768; i += 16)
  {
    __m256i x = _mm256_cvtepi8_epi16(_mm_loadu_si128((__m128i *)&r[i]));
    x &= _mm256_set1_epi32(0x00010001);
    tally = _mm256_add_epi16(tally, x);
  }

  tally = _mm256_hadd_epi16(tally, tally);
  tally = _mm256_hadd_epi16(tally, tally);
  tally = _mm256_hadd_epi16(tally, tally);

  weight = _mm_extract_epi16(_mm256_extracti128_si256(tally, 0), 0)
      + _mm_extract_epi16(_mm256_extracti128_si256(tally, 1), 0);

  weight -= w;

  return (-(crypto_int32)(crypto_uint16)weight) >> 30;
}

#endif
