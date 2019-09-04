#if __AVX2__
#include <immintrin.h>
#include "params.h"
#include <sodium/crypto_uint32.h>
#include "rq.h"

#define alpha_top _mm256_set1_epi32(0x43380000)
#define alpha _mm256_set1_pd(6755399441055744.0)
#define v10923_16 _mm256_set1_epi16(10923)
#define floor(x) _mm256_floor_pd(x)

void
rq_roundencode(unsigned char *c, const modq *f)
{
  int i;
  __m256i h[50];

  for(i = 0; i < 208; i += 16)
  {
    __m256i a0, a1, a2, b0, b1, b2, c0, c1, c2, d0, d1, d2;
    __m256i e0, e1, f0, f1, g0, g1;
    a0 = _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&f[0]));
    a1 = _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&f[8]));
    a2 = _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)&f[16]));
    a0 = _mm256_inserti128_si256(a0, _mm_loadu_si128((__m128i *)&f[24]), 1);
    a1 = _mm256_inserti128_si256(a1, _mm_loadu_si128((__m128i *)&f[32]), 1);
    a2 = _mm256_inserti128_si256(a2, _mm_loadu_si128((__m128i *)&f[40]), 1);
    f += 48;

    a0 = _mm256_mulhrs_epi16(a0, v10923_16);
    a1 = _mm256_mulhrs_epi16(a1, v10923_16);
    a2 = _mm256_mulhrs_epi16(a2, v10923_16);

    /* a0: a0 a1 a2 b0 b1 b2 c0 c1 and similar second half */
    /* a1: c2 d0 d1 d2 e0 e1 e2 f0 */
    /* a2: f1 f2 g0 g1 g2 h0 h1 h2 */

    b1 = _mm256_blend_epi16(a2, a0, 0xf0);
    b1 = _mm256_shuffle_epi32(b1, 0x4e);
    b0 = _mm256_blend_epi16(a0, a1, 0xf0);
    b2 = _mm256_blend_epi16(a1, a2, 0xf0);
    /* XXX: use shufps instead? */

    /* b0: a0 a1 a2 b0 e0 e1 e2 f0 */
    /* b1: b1 b2 c0 c1 f1 f2 g0 g1 */
    /* b2: c2 d0 d1 d2 g2 h0 h1 h2 */

    c1 = _mm256_blend_epi16(b2, b0, 0xcc);
    c1 = _mm256_shuffle_epi32(c1, 0xb1);
    c0 = _mm256_blend_epi16(b0, b1, 0xcc);
    c2 = _mm256_blend_epi16(b1, b2, 0xcc);

    /* c0: a0 a1 c0 c1 e0 e1 g0 g1 */
    /* c1: a2 b0 c2 d0 e2 f0 g2 h0 */
    /* c2: b1 b2 d1 d2 f1 f2 h1 h2 */

    d1 = _mm256_blend_epi16(c2, c0, 0xaa);
    d1 = _mm256_shufflelo_epi16(d1, 0xb1);
    d1 = _mm256_shufflehi_epi16(d1, 0xb1);
    d0 = _mm256_blend_epi16(c0, c1, 0xaa);
    d2 = _mm256_blend_epi16(c1, c2, 0xaa);

    /* d0: a0 b0 c0 d0 e0 f0 g0 h0 */
    /* d1: a1 b1 c1 d1 e1 f1 g1 h1 */
    /* d2: a2 b2 c2 d2 e2 f2 g2 h2 */

    d0 = _mm256_add_epi16(d0, _mm256_set1_epi16(765));
    d1 = _mm256_add_epi16(d1, _mm256_set1_epi16(765));
    d2 = _mm256_add_epi16(d2, _mm256_set1_epi16(765));
    /* want bytes of d0 + 1536*d1 + 1536*1536*d2 */

    e0 = d0 & _mm256_set1_epi16(0xff);
    d0 = _mm256_srli_epi16(d0, 8);
    /* want e0, d0 + 6*d1 + 6*1536*d2 */

    d1 = _mm256_mullo_epi16(d1, _mm256_set1_epi16(6));
    d0 = _mm256_add_epi16(d0, d1);
    /* want e0, d0 + 6*1536*d2 */

    e1 = _mm256_slli_epi16(d0, 8);
    e0 = _mm256_add_epi16(e0, e1);
    d0 = _mm256_srli_epi16(d0, 8);
    /* want e0, d0 + 36*d2 */

    d2 = _mm256_mullo_epi16(d2, _mm256_set1_epi16(36));
    e1 = _mm256_add_epi16(d0, d2);
    /* want e0, e1 */

    /* e0: out0 out1 out4 out5 out8 out9 ... */
    /* e1: out2 out3 out6 out7 out10 out11 ... */

    f0 = _mm256_unpacklo_epi16(e0, e1);
    f1 = _mm256_unpackhi_epi16(e0, e1);

    g0 = _mm256_permute2x128_si256(f0, f1, 0x20);
    g1 = _mm256_permute2x128_si256(f0, f1, 0x31);

    _mm256_storeu_si256((__m256i *)c, g0);
    _mm256_storeu_si256((__m256i *)(c + 32), g1);
    c += 64;
  }

  for(i = 0; i < 9; ++i)
  {
    __m256i x = _mm256_loadu_si256((__m256i *)&f[16 * i]);
    _mm256_storeu_si256(&h[i], _mm256_mulhrs_epi16(x, v10923_16));
  }
  f = (const modq *)h;

  for(i = 208; i < 253; ++i)
  {
    crypto_int32 f0, f1, f2;
    f0 = *f++;
    f1 = *f++;
    f2 = *f++;
    f0 += 1806037245;
    f1 *= 3;
    f2 *= 9;
    f0 += f1 << 9;
    f0 += f2 << 18;
    *(crypto_int32 *)c = f0;
    c += 4;
  }
  {
    crypto_int32 f0, f1;
    f0 = *f++;
    f1 = *f++;
    f0 += 1175805;
    f1 *= 3;
    f0 += f1 << 9;
    *c++ = f0;
    f0 >>= 8;
    *c++ = f0;
    f0 >>= 8;
    *c++ = f0;
  }
}

void
rq_decoderounded(modq *f, const unsigned char *c)
{
  crypto_uint32 c0, c1, c2, c3;
  crypto_uint32 f0, f1, f2;
  int i;

  for(i = 0; i < 248; i += 8)
  {
    __m256i abcdefgh, todo[2];
    __m256d x, f2, f1, f0;
    __m128i if2, if1, if0;
    int j;

    abcdefgh = _mm256_loadu_si256((__m256i *)c);
    c += 32;

    todo[0] = _mm256_unpacklo_epi32(abcdefgh, alpha_top);
    todo[1] = _mm256_unpackhi_epi32(abcdefgh, alpha_top);

    for(j = 0; j < 2; ++j)
    {
      x = *(__m256d *)&todo[j];
      x -= alpha;

      /* x is f0 + f1*1536 + f2*1536^2 */
      /* with each f between 0 and 1530 */

      f2 =
          x
          * _mm256_set1_pd(
              0.00000042385525173611114052197733521876177320564238470979034900665283203125);
      f2 = floor(f2);
      x -= f2 * _mm256_set1_pd(2359296.0);

      f1 =
          x
          * _mm256_set1_pd(
              0.00065104166666666673894681149903362893383018672466278076171875);
      f1 = floor(f1);
      x -= f1 * _mm256_set1_pd(1536.0);

      f0 = x;

      f2 -=
          _mm256_set1_pd(1531.0)
          * floor(
              f2
              * _mm256_set1_pd(
                  0.0006531678641410842804659875326933615724556148052215576171875));
      f1 -=
          _mm256_set1_pd(1531.0)
          * floor(
              f1
              * _mm256_set1_pd(
                  0.0006531678641410842804659875326933615724556148052215576171875));
      f0 -=
          _mm256_set1_pd(1531.0)
          * floor(
              f0
              * _mm256_set1_pd(
                  0.0006531678641410842804659875326933615724556148052215576171875));

      f2 *= _mm256_set1_pd(3.0);
      f2 -= _mm256_set1_pd(2295.0);
      f1 *= _mm256_set1_pd(3.0);
      f1 -= _mm256_set1_pd(2295.0);
      f0 *= _mm256_set1_pd(3.0);
      f0 -= _mm256_set1_pd(2295.0);

      if2 = _mm256_cvtpd_epi32(f2); /* a2 b2 e2 f2 */
      if1 = _mm256_cvtpd_epi32(f1); /* a1 b1 e1 f1 */
      if0 = _mm256_cvtpd_epi32(f0); /* a0 b0 e0 f0 */

      f[6 * j + 0] = _mm_extract_epi32(if0, 0);
      f[6 * j + 1] = _mm_extract_epi32(if1, 0);
      f[6 * j + 2] = _mm_extract_epi32(if2, 0);
      f[6 * j + 3] = _mm_extract_epi32(if0, 1);
      f[6 * j + 4] = _mm_extract_epi32(if1, 1);
      f[6 * j + 5] = _mm_extract_epi32(if2, 1);

      f[6 * j + 12] = _mm_extract_epi32(if0, 2);
      f[6 * j + 13] = _mm_extract_epi32(if1, 2);
      f[6 * j + 14] = _mm_extract_epi32(if2, 2);
      f[6 * j + 15] = _mm_extract_epi32(if0, 3);
      f[6 * j + 16] = _mm_extract_epi32(if1, 3);
      f[6 * j + 17] = _mm_extract_epi32(if2, 3);
    }

    f += 24;
  }

  for(i = 248; i < 253; ++i)
  {
    c0 = *c++;
    c1 = *c++;
    c2 = *c++;
    c3 = *c++;

    /* f0 + f1*1536 + f2*1536^2 */
    /* = c0 + c1*256 + c2*256^2 + c3*256^3 */
    /* with each f between 0 and 1530 */

    /* f2 = (64/9)c3 + (1/36)c2 + (1/9216)c1 + (1/2359296)c0 - [0,0.99675] */
    /* claim: 2^21 f2 < x < 2^21(f2+1) */
    /* where x = 14913081*c3 + 58254*c2 + 228*(c1+2) */
    /* proof: x - 2^21 f2 = 456 - (8/9)c0 + (4/9)c1 - (2/9)c2 + (1/9)c3 + 2^21
     * [0,0.99675] */
    /* at least 456 - (8/9)255 - (2/9)255 > 0 */
    /* at most 456 + (4/9)255 + (1/9)255 + 2^21 0.99675 < 2^21 */
    f2 = (14913081 * c3 + 58254 * c2 + 228 * (c1 + 2)) >> 21;

    c2 += c3 << 8;
    c2 -= (f2 * 9) << 2;
    /* f0 + f1*1536 */
    /* = c0 + c1*256 + c2*256^2 */
    /* c2 <= 35 = floor((1530+1530*1536)/256^2) */
    /* f1 = (128/3)c2 + (1/6)c1 + (1/1536)c0 - (1/1536)f0 */
    /* claim: 2^21 f1 < x < 2^21(f1+1) */
    /* where x = 89478485*c2 + 349525*c1 + 1365*(c0+1) */
    /* proof: x - 2^21 f1 = 1365 - (1/3)c2 - (1/3)c1 - (1/3)c0 + (4096/3)f0 */
    /* at least 1365 - (1/3)35 - (1/3)255 - (1/3)255 > 0 */
    /* at most 1365 + (4096/3)1530 < 2^21 */
    f1 = (89478485 * c2 + 349525 * c1 + 1365 * (c0 + 1)) >> 21;

    c1 += c2 << 8;
    c1 -= (f1 * 3) << 1;

    c0 += c1 << 8;
    f0 = c0;

    *f++ = modq_freeze(f0 * 3 + q - qshift);
    *f++ = modq_freeze(f1 * 3 + q - qshift);
    *f++ = modq_freeze(f2 * 3 + q - qshift);
  }

  c0 = *c++;
  c1 = *c++;
  c2 = *c++;

  f1 = (89478485 * c2 + 349525 * c1 + 1365 * (c0 + 1)) >> 21;

  c1 += c2 << 8;
  c1 -= (f1 * 3) << 1;

  c0 += c1 << 8;
  f0 = c0;

  *f++ = modq_freeze(f0 * 3 + q - qshift);
  *f++ = modq_freeze(f1 * 3 + q - qshift);
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
}
#endif
