#if __AVX2__
#include <immintrin.h>
#include "params.h"
#include <sodium/crypto_uint32.h>
#include <sodium/crypto_int64.h>
#include "rq.h"

#define v2295_16 _mm256_set1_epi16(2295)
#define v2295_16_128 _mm_set1_epi16(2295)
#define alpha_top _mm256_set1_epi32(0x43380000)
#define alpha _mm256_set1_pd(6755399441055744.0)
#define alpha_64 _mm256_set1_epi64(0x4338000000000000)

/* each reciprocal is rounded _up_ to nearest floating-point number */
#define recip54 0.0185185185185185209599811884118025773204863071441650390625
#define recip4591 \
  0.000217817468961010681817447309782664888189174234867095947265625
#define recip6144 \
  0.0001627604166666666847367028747584072334575466811656951904296875
#define recip331776 \
  0.00000301408179012345704632478034235010255770248477347195148468017578125
#define recip37748736 \
  0.000000026490953233506946282623583451172610825352649044361896812915802001953125

#define broadcast(r) _mm256_set1_pd(r)
#define floor(x) _mm256_floor_pd(x)

// 32-bit hosts only
#ifndef __amd64__
#define _mm_extract_epi64(X, N) \
  (__extension__({              \
    __v2di __a = (__v2di)(X);   \
    __a[N];                     \
  }))
#endif

void
rq_encode(unsigned char *c, const modq *f)
{
  crypto_int32 f0, f1, f2, f3, f4;
  int i;

  for(i = 0; i < p / 5; ++i)
  {
    f0 = *f++ + qshift;
    f1 = *f++ + qshift;
    f2 = *f++ + qshift;
    f3 = *f++ + qshift;
    f4 = *f++ + qshift;
    /* now want f0 + 6144*f1 + ... as a 64-bit integer */
    f1 *= 3;
    f2 *= 9;
    f3 *= 27;
    f4 *= 81;
    /* now want f0 + f1<<11 + f2<<22 + f3<<33 + f4<<44 */
    f0 += f1 << 11;
    *c++ = f0;
    f0 >>= 8;
    *c++ = f0;
    f0 >>= 8;
    f0 += f2 << 6;
    *c++ = f0;
    f0 >>= 8;
    *c++ = f0;
    f0 >>= 8;
    f0 += f3 << 1;
    *c++ = f0;
    f0 >>= 8;
    f0 += f4 << 4;
    *c++ = f0;
    f0 >>= 8;
    *c++ = f0;
    f0 >>= 8;
    *c++ = f0;
  }
  /* XXX: using p mod 5 = 1 */
  f0   = *f++ + qshift;
  *c++ = f0;
  f0 >>= 8;
  *c++ = f0;
}

void
rq_decode(modq *f, const unsigned char *c)
{
  crypto_uint32 c0, c1;
  int i;

  for(i = 0; i < 152; i += 4)
  {
    __m256i abcd, ac, bd, abcd0, abcd1;
    __m256d x0, x1, f4, f3, f2, f1, f0;
    __m128i if4, if3, if2, if1, if0;
    __m128i x01, x23, x02, x13, xab, xcd;

    /* f0 + f1*6144 + f2*6144^2 + f3*6144^3 + f4*6144^4 */
    /* = c0 + c1*256 + ... + c6*256^6 + c7*256^7 */
    /* with each f between 0 and 4590 */

    /* could use _mm256_cvtepi32_pd instead; but beware uint32 */

    abcd = _mm256_loadu_si256((__m256i *)c); /* a0 a1 b0 b1 c0 c1 d0 d1 */
    c += 32;

    ac    = _mm256_unpacklo_epi32(abcd, alpha_top); /* a0 a1 c0 c1 */
    bd    = _mm256_unpackhi_epi32(abcd, alpha_top); /* b0 b1 d0 d1 */
    abcd1 = _mm256_unpackhi_epi64(ac, bd);          /* a1 b1 c1 d1 */
    abcd0 = _mm256_unpacklo_epi64(ac, bd);          /* a0 b0 c0 d0 */
    x1    = *(__m256d *)&abcd1;
    x0    = *(__m256d *)&abcd0;

    x1 -= alpha;
    x0 -= alpha;

    /* x1 is [0,41] + [0,4590]*54 + f4*331776 */
    f4 = broadcast(recip331776) * x1;
    f4 = floor(f4);
    x1 -= broadcast(331776.0) * f4;

    /* x1 is [0,41] + f3*54 */
    f3 = broadcast(recip54) * x1;
    f3 = floor(f3);
    x1 -= broadcast(54.0) * f3;

    x0 += broadcast(4294967296.0) * x1;

    /* x0 is [0,4590] + [0,4590]*6144 + f2*6144^2 */
    f2 = broadcast(recip37748736) * x0;
    f2 = floor(f2);
    x0 -= broadcast(37748736.0) * f2;

    /* x0 is [0,4590] + f1*6144 */
    f1 = broadcast(recip6144) * x0;
    f1 = floor(f1);
    x0 -= broadcast(6144.0) * f1;

    f0 = x0;

    f4 -= broadcast(4591.0) * floor(broadcast(recip4591) * f4);
    f3 -= broadcast(4591.0) * floor(broadcast(recip4591) * f3);
    f2 -= broadcast(4591.0) * floor(broadcast(recip4591) * f2);
    f1 -= broadcast(4591.0) * floor(broadcast(recip4591) * f1);
    f0 -= broadcast(4591.0) * floor(broadcast(recip4591) * f0);

    if4 = _mm256_cvtpd_epi32(f4); /* a4 0 b4 0 c4 0 d4 0 */
    if3 = _mm256_cvtpd_epi32(f3); /* a3 0 b3 0 c3 0 d3 0 */
    if2 = _mm256_cvtpd_epi32(f2); /* a2 0 b2 0 c2 0 d2 0 */
    if1 = _mm256_cvtpd_epi32(f1); /* a1 0 b1 0 c1 0 d1 0 */
    if0 = _mm256_cvtpd_epi32(f0); /* a0 0 b0 0 c0 0 d0 0 */

    if4   = _mm_sub_epi16(if4, v2295_16_128);
    f[4]  = _mm_extract_epi32(if4, 0);
    f[9]  = _mm_extract_epi32(if4, 1);
    f[14] = _mm_extract_epi32(if4, 2);
    f[19] = _mm_extract_epi32(if4, 3);

    x23 = _mm_packs_epi32(if2, if3);    /* a2 b2 c2 d2 a3 b3 c3 d3 */
    x01 = _mm_packs_epi32(if0, if1);    /* a0 b0 c0 d0 a1 b1 c1 d1 */
    x02 = _mm_unpacklo_epi16(x01, x23); /* a0 a2 b0 b2 c0 c2 d0 d2 */
    x13 = _mm_unpackhi_epi16(x01, x23); /* a1 a3 b1 b3 c1 c3 d1 d3 */
    xab = _mm_unpacklo_epi16(x02, x13); /* a0 a1 a2 a3 b0 b1 b2 b3 */
    xcd = _mm_unpackhi_epi16(x02, x13); /* c0 c1 c2 c3 d0 d1 d2 d3 */
    xab = _mm_sub_epi16(xab, v2295_16_128);
    xcd = _mm_sub_epi16(xcd, v2295_16_128);

    *(crypto_int64 *)(f + 0)  = _mm_extract_epi64(xab, 0);
    *(crypto_int64 *)(f + 5)  = _mm_extract_epi64(xab, 1);
    *(crypto_int64 *)(f + 10) = _mm_extract_epi64(xcd, 0);
    *(crypto_int64 *)(f + 15) = _mm_extract_epi64(xcd, 1);
    f += 20;
  }

  c0 = *c++;
  c1 = *c++;
  c0 += c1 << 8;
  *f++ = modq_freeze(c0 + q - qshift);
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
  *f++ = 0;
}
#endif
