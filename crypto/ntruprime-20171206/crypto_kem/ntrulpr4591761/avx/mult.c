#include <string.h>
#include <immintrin.h>
#include "rq.h"

#define MULSTEP_gcc(j,h0,h1,h2,h3,h4) \
  gj = g[j]; \
  h0 += f0 * gj; \
  _mm256_storeu_ps(&h[i + j],h0); \
  h1 += f1 * gj; \
  h2 += f2 * gj; \
  h3 += f3 * gj; \
  h4 += f4 * gj; \
  h0 = _mm256_loadu_ps(&h[i + j + 5]); \
  h0 += f5 * gj;

#define MULSTEP_asm(j,h0,h1,h2,h3,h4) \
  gj = g[j]; \
  __asm__( \
    "vfmadd231ps %5,%6,%0 \n\t" \
    "vmovups %0,%12 \n\t" \
    "vmovups %13,%0 \n\t" \
    "vfmadd231ps %5,%7,%1 \n\t" \
    "vfmadd231ps %5,%8,%2 \n\t" \
    "vfmadd231ps %5,%9,%3 \n\t" \
    "vfmadd231ps %5,%10,%4 \n\t" \
    "vfmadd231ps %5,%11,%0 \n\t" \
    : "+x"(h0),"+x"(h1),"+x"(h2),"+x"(h3),"+x"(h4) \
    : "x"(gj),"x"(f0),"x"(f1),"x"(f2),"x"(f3),"x"(f4),"x"(f5),"m"(h[i+j]),"m"(h[i+j+5]));

#define MULSTEP MULSTEP_asm

#define MULSTEP_noload(j,h0,h1,h2,h3,h4) \
  gj = g[j]; \
  __asm__( \
    "vfmadd231ps %5,%6,%0 \n\t" \
    "vmovups %0,%12 \n\t" \
    "vfmadd231ps %5,%7,%1 \n\t" \
    "vfmadd231ps %5,%8,%2 \n\t" \
    "vfmadd231ps %5,%9,%3 \n\t" \
    "vfmadd231ps %5,%10,%4 \n\t" \
    "vmulps %5,%11,%0 \n\t" \
    : "+x"(h0),"+x"(h1),"+x"(h2),"+x"(h3),"+x"(h4) \
    : "x"(gj),"x"(f0),"x"(f1),"x"(f2),"x"(f3),"x"(f4),"x"(f5),"m"(h[i+j]));

#define MULSTEP_fromzero(j,h0,h1,h2,h3,h4) \
  gj = g[j]; \
  __asm__( \
    "vmulps %5,%6,%0 \n\t" \
    "vmovups %0,%12 \n\t" \
    "vmulps %5,%7,%1 \n\t" \
    "vmulps %5,%8,%2 \n\t" \
    "vmulps %5,%9,%3 \n\t" \
    "vmulps %5,%10,%4 \n\t" \
    "vmulps %5,%11,%0 \n\t" \
    : "=&x"(h0),"=&x"(h1),"=&x"(h2),"=&x"(h3),"=&x"(h4) \
    : "x"(gj),"x"(f0),"x"(f1),"x"(f2),"x"(f3),"x"(f4),"x"(f5),"m"(h[i+j]));

static inline __m128i _mm_load_cvtepi8_epi16(const long long *x)
{
  __m128i result;
  __asm__("vpmovsxbw %1, %0" : "=x"(result) : "m"(*x));
  return result;
}

#define v0 _mm256_set1_epi32(0)
#define v0_128 _mm_set1_epi32(0)
#define v7 _mm256_set1_epi16(7)
#define v4591_16 _mm256_set1_epi16(4591)
#define v2296_16 _mm256_set1_epi16(2296)

#define alpha_32 _mm256_set1_epi32(0x4b400000)
#define alpha_32_128 _mm_set1_epi32(0x4b400000)
#define alpha_float _mm256_set1_ps(12582912.0)

#define v0_float _mm256_set1_ps(0)
#define v1_float _mm256_set1_ps(1)
#define vm1_float _mm256_set1_ps(-1)
#define vm4591_float _mm256_set1_ps(-4591)
#define recip4591_float _mm256_set1_ps(0.00021781746896101067305597908952297974298)

static inline __m256 add(__m256 x,__m256 y)
{
  return x + y;
}

static inline __m256 fastadd(__m256 x,__m256 y)
{
  return _mm256_fmadd_ps(y,v1_float,x);
}

static inline __m256 fastsub(__m256 x,__m256 y)
{
  return _mm256_fmadd_ps(y,vm1_float,x);
}

static inline __m256 reduce(__m256 x)
{
  __m256 q = x * recip4591_float;
  q = _mm256_round_ps(q,8);
  return _mm256_fmadd_ps(q,vm4591_float,x);
}

static inline __m256i squeeze(__m256i x)
{
  __m256i q = _mm256_mulhrs_epi16(x,v7);
  q = _mm256_mullo_epi16(q,v4591_16);
  return _mm256_sub_epi16(x,q);
}

static inline __m256i squeezeadd16(__m256i x,__m256i y)
{
  __m256i q;
  x = _mm256_add_epi16(x,y);
  q = _mm256_mulhrs_epi16(x,v7);
  q = _mm256_mullo_epi16(q,v4591_16);
  return _mm256_sub_epi16(x,q);
}

static inline __m256i freeze(__m256i x)
{
  __m256i mask, x2296, x4591;
  x4591 = _mm256_add_epi16(x,v4591_16);
  mask = _mm256_srai_epi16(x,15);
  x = _mm256_blendv_epi8(x,x4591,mask);
  x2296 = _mm256_sub_epi16(x,v2296_16);
  mask = _mm256_srai_epi16(x2296,15);
  x4591 = _mm256_sub_epi16(x,v4591_16);
  x = _mm256_blendv_epi8(x4591,x,mask);
  return x;
}

/* 24*8*float32 f inputs between -10000 and 10000 */
/* 24*8*float32 g inputs between -32 and 32 */
/* 48*8*float32 h outputs between -7680000 and 7680000 */
static void mult24x8_float(__m256 h[48],const __m256 f[24],const __m256 g[24])
{
  int i, j;
  __m256 f0, f1, f2, f3, f4, f5, gj, h0, h1, h2, h3, h4;

  i = 0;
  f0 = f[i];
  f1 = f[i + 1];
  f2 = f[i + 2];
  f3 = f[i + 3];
  f4 = f[i + 4];
  f5 = f[i + 5];
  MULSTEP_fromzero(0,h0,h1,h2,h3,h4)
  for (j = 0;j < 20;j += 5) {
    MULSTEP_noload(j + 1,h1,h2,h3,h4,h0)
    MULSTEP_noload(j + 2,h2,h3,h4,h0,h1)
    MULSTEP_noload(j + 3,h3,h4,h0,h1,h2)
    MULSTEP_noload(j + 4,h4,h0,h1,h2,h3)
    MULSTEP_noload(j + 5,h0,h1,h2,h3,h4)
  }
  MULSTEP_noload(j + 1,h1,h2,h3,h4,h0)
  MULSTEP_noload(j + 2,h2,h3,h4,h0,h1)
  MULSTEP_noload(j + 3,h3,h4,h0,h1,h2)
  h[i + j + 4] = h4;
  h[i + j + 5] = h0;
  h[i + j + 6] = h1;
  h[i + j + 7] = h2;
  h[i + j + 8] = h3;

  for (i = 6;i < 24;i += 6) {
    f0 = f[i];
    f1 = f[i + 1];
    f2 = f[i + 2];
    f3 = f[i + 3];
    f4 = f[i + 4];
    f5 = f[i + 5];
    h0 = h[i];
    h1 = h[i + 1];
    h2 = h[i + 2];
    h3 = h[i + 3];
    h4 = h[i + 4];
    for (j = 0;j < 15;j += 5) {
      MULSTEP(j + 0,h0,h1,h2,h3,h4)
      MULSTEP(j + 1,h1,h2,h3,h4,h0)
      MULSTEP(j + 2,h2,h3,h4,h0,h1)
      MULSTEP(j + 3,h3,h4,h0,h1,h2)
      MULSTEP(j + 4,h4,h0,h1,h2,h3)
    }
    MULSTEP(j + 0,h0,h1,h2,h3,h4)
    MULSTEP(j + 1,h1,h2,h3,h4,h0)
    MULSTEP(j + 2,h2,h3,h4,h0,h1)
    MULSTEP_noload(j + 3,h3,h4,h0,h1,h2)
    MULSTEP_noload(j + 4,h4,h0,h1,h2,h3)
    MULSTEP_noload(j + 5,h0,h1,h2,h3,h4)
    MULSTEP_noload(j + 6,h1,h2,h3,h4,h0)
    MULSTEP_noload(j + 7,h2,h3,h4,h0,h1)
    MULSTEP_noload(j + 8,h3,h4,h0,h1,h2)
    h[i + j + 9] = h4;
    h[i + j + 10] = h0;
    h[i + j + 11] = h1;
    h[i + j + 12] = h2;
    h[i + j + 13] = h3;
  }

  h[47] = v0_float;
}

/* 48*8*float32 f inputs between -5000 and 5000 */
/* 48*8*float32 g inputs between -16 and 16 */
/* 96*8*float32 h outputs between -3840000 and 3840000 */
static void mult48x8_float(__m256 h[96],const __m256 f[48],const __m256 g[48])
{
  __m256 h01[48];
  __m256 g01[24];
  __m256 *f01 = h01 + 24;
  int i;

  for (i = 24;i > 0;) {
    i -= 2;
    f01[i] = f[i] + f[i + 24];
    g01[i] = g[i] + g[i + 24];
    f01[i + 1] = f[i + 1] + f[i + 1 + 24];
    g01[i + 1] = g[i + 1] + g[i + 1 + 24];
  }

  mult24x8_float(h,f,g);
  mult24x8_float(h + 48,f + 24,g + 24);
  mult24x8_float(h01,f01,g01);

  for (i = 0;i < 24;++i) {
    __m256 h0i = h[i];
    __m256 h0itop = h[i + 24];
    __m256 h1i = h[i + 48];
    __m256 h1itop = h[i + 72];
    __m256 h01i = h01[i];
    __m256 h01itop = h01[i + 24];
    __m256 c = fastsub(h0itop,h1i);
    h[i + 24] = c + fastsub(h01i,h0i);
    h[i + 48] = fastsub(h01itop,h1itop) - c;
  }
}

/* 96*8*float32 f inputs between -2500 and 2500 */
/* 96*8*float32 g inputs between -8 and 8 */
/* 192*8*float32 h outputs between -1920000 and 1920000 */
static void mult96x8_float(__m256 h[192],const __m256 f[96],const __m256 g[96])
{
  __m256 h01[96];
  __m256 g01[48];
  __m256 *f01 = h01 + 48;
  int i;

  for (i = 48;i > 0;) {
    i -= 4;
    f01[i] = f[i] + f[i + 48];
    g01[i] = g[i] + g[i + 48];
    f01[i + 1] = f[i + 1] + f[i + 1 + 48];
    g01[i + 1] = g[i + 1] + g[i + 1 + 48];
    f01[i + 2] = f[i + 2] + f[i + 2 + 48];
    g01[i + 2] = g[i + 2] + g[i + 2 + 48];
    f01[i + 3] = f[i + 3] + f[i + 3 + 48];
    g01[i + 3] = g[i + 3] + g[i + 3 + 48];
  }

  mult48x8_float(h,f,g);
  mult48x8_float(h + 96,f + 48,g + 48);
  mult48x8_float(h01,f01,g01);

  for (i = 0;i < 48;++i) {
    __m256 h0i = h[i];
    __m256 h0itop = h[i + 48];
    __m256 h1i = h[i + 96];
    __m256 h1itop = h[i + 144];
    __m256 h01i = h01[i];
    __m256 h01itop = h01[i + 48];
    __m256 c = fastsub(h0itop,h1i);
    h[i + 48] = c + fastsub(h01i,h0i);
    h[i + 96] = fastsub(h01itop,h1itop) - c;
  }
}

/* 96*16*int16 f inputs between -2500 and 2500 */
/* 96*(16*int8 stored in 32*int8) g inputs between -8 and 8 */
/* 192*16*int16 h outputs between -2400 and 2400 */
static void mult96x16(__m256i h[192],const __m256i f[96],const __m256i g[96])
{
  __m256 hfloat[192];
  __m256 gfloat[96];
  __m256 *ffloat = hfloat + 96;
  int i, p;

  for (p = 0;p < 2;++p) {
    for (i = 96;i > 0;) {
      i -= 2;
      __m256i fi = _mm256_cvtepi16_epi32(_mm_loadu_si128(p + (const __m128i *) &f[i]));
      __m256i gi = _mm256_cvtepi16_epi32(_mm_load_cvtepi8_epi16(p + (const long long *) &g[i]));
      __m256 storage;
      *(__m256i *) &storage = _mm256_add_epi32(fi,alpha_32);
      ffloat[i] = storage - alpha_float;
      *(__m256i *) &storage = _mm256_add_epi32(gi,alpha_32);
      gfloat[i] = storage - alpha_float;
      fi = _mm256_cvtepi16_epi32(_mm_loadu_si128(p + (const __m128i *) &f[i + 1]));
      gi = _mm256_cvtepi16_epi32(_mm_load_cvtepi8_epi16(p + (const long long *) &g[i + 1]));
      *(__m256i *) &storage = _mm256_add_epi32(fi,alpha_32);
      ffloat[i + 1] = storage - alpha_float;
      *(__m256i *) &storage = _mm256_add_epi32(gi,alpha_32);
      gfloat[i + 1] = storage - alpha_float;
    }
    mult96x8_float(hfloat,ffloat,gfloat);
    for (i = 192;i > 0;) {
      __m128i h0, h1;
      i -= 4;
      hfloat[i] = add(alpha_float,reduce(hfloat[i]));
      hfloat[i + 1] = fastadd(alpha_float,reduce(hfloat[i + 1]));
      hfloat[i + 2] = add(alpha_float,reduce(hfloat[i + 2]));
      hfloat[i + 3] = fastadd(alpha_float,reduce(hfloat[i + 3]));
      h0 = 0[(__m128i *) &hfloat[i]]; h0 = _mm_sub_epi32(h0,alpha_32_128);
      h1 = 1[(__m128i *) &hfloat[i]]; h1 = _mm_sub_epi32(h1,alpha_32_128);
      _mm_storeu_si128(p + (__m128i *) &h[i],_mm_packs_epi32(h0,h1));
      h0 = 0[(__m128i *) &hfloat[i + 1]]; h0 = _mm_sub_epi32(h0,alpha_32_128);
      h1 = 1[(__m128i *) &hfloat[i + 1]]; h1 = _mm_sub_epi32(h1,alpha_32_128);
      _mm_storeu_si128(p + (__m128i *) &h[i + 1],_mm_packs_epi32(h0,h1));
      h0 = 0[(__m128i *) &hfloat[i + 2]]; h0 = _mm_sub_epi32(h0,alpha_32_128);
      h1 = 1[(__m128i *) &hfloat[i + 2]]; h1 = _mm_sub_epi32(h1,alpha_32_128);
      _mm_storeu_si128(p + (__m128i *) &h[i + 2],_mm_packs_epi32(h0,h1));
      h0 = 0[(__m128i *) &hfloat[i + 3]]; h0 = _mm_sub_epi32(h0,alpha_32_128);
      h1 = 1[(__m128i *) &hfloat[i + 3]]; h1 = _mm_sub_epi32(h1,alpha_32_128);
      _mm_storeu_si128(p + (__m128i *) &h[i + 3],_mm_packs_epi32(h0,h1));
    }
  }
}

/* int16 i of output x[j] is int16 j of input x[i] */
static void transpose16(__m256i x[16])
{
  const static int rev[4] = {0,4,2,6};
  int i;
  __m256i y[16];

  for (i = 0;i < 16;i += 4) {
    __m256i a0 = x[i]; 
    __m256i a1 = x[i + 1];
    __m256i a2 = x[i + 2]; 
    __m256i a3 = x[i + 3];
    __m256i b0 = _mm256_unpacklo_epi16(a0,a1);
    __m256i b1 = _mm256_unpackhi_epi16(a0,a1);
    __m256i b2 = _mm256_unpacklo_epi16(a2,a3);
    __m256i b3 = _mm256_unpackhi_epi16(a2,a3);
    __m256i c0 = _mm256_unpacklo_epi32(b0,b2);
    __m256i c2 = _mm256_unpackhi_epi32(b0,b2);
    __m256i c1 = _mm256_unpacklo_epi32(b1,b3);
    __m256i c3 = _mm256_unpackhi_epi32(b1,b3);
    y[i] = c0;
    y[i + 2] = c2;
    y[i + 1] = c1;
    y[i + 3] = c3;
  }
  for (i = 0;i < 4;++i) {
    int r = rev[i];
    __m256i c0 = y[i];
    __m256i c4 = y[i + 4];
    __m256i c8 = y[i + 8];
    __m256i c12 = y[i + 12];
    __m256i d0 = _mm256_unpacklo_epi64(c0,c4);
    __m256i d4 = _mm256_unpackhi_epi64(c0,c4);
    __m256i d8 = _mm256_unpacklo_epi64(c8,c12);
    __m256i d12 = _mm256_unpackhi_epi64(c8,c12);
    __m256i e0 = _mm256_permute2x128_si256(d0,d8,0x20);
    __m256i e8 = _mm256_permute2x128_si256(d0,d8,0x31);
    __m256i e4 = _mm256_permute2x128_si256(d4,d12,0x20);
    __m256i e12 = _mm256_permute2x128_si256(d4,d12,0x31);
    x[r] = e0;
    x[r + 8] = e8;
    x[r + 1] = e4;
    x[r + 9] = e12;
  }
}

/* byte i of output x[j] is byte j of input x[i] */
static void transpose32(__m256i x[32])
{
  const static int rev[4] = {0,8,4,12};
  int i;
  __m256i y[32];

  for (i = 0;i < 32;i += 4) {
    __m256i a0 = x[i]; 
    __m256i a1 = x[i + 1];
    __m256i a2 = x[i + 2];
    __m256i a3 = x[i + 3];
    __m256i b0 = _mm256_unpacklo_epi8(a0,a1);
    __m256i b1 = _mm256_unpackhi_epi8(a0,a1);
    __m256i b2 = _mm256_unpacklo_epi8(a2,a3);
    __m256i b3 = _mm256_unpackhi_epi8(a2,a3);
    __m256i c0 = _mm256_unpacklo_epi16(b0,b2);
    __m256i c2 = _mm256_unpackhi_epi16(b0,b2);
    __m256i c1 = _mm256_unpacklo_epi16(b1,b3);
    __m256i c3 = _mm256_unpackhi_epi16(b1,b3);
    y[i] = c0;
    y[i + 2] = c2;
    y[i + 1] = c1;
    y[i + 3] = c3;
  }
  for (i = 0;i < 4;++i) {
    int r = rev[i];
    __m256i c0 = y[i];
    __m256i c8 = y[i + 8];
    __m256i c16 = y[i + 16];
    __m256i c24 = y[i + 24];
    __m256i c4 = y[i + 4];
    __m256i c12 = y[i + 12];
    __m256i c20 = y[i + 20];
    __m256i c28 = y[i + 28];
    __m256i d0 = _mm256_unpacklo_epi32(c0,c4);
    __m256i d4 = _mm256_unpackhi_epi32(c0,c4);
    __m256i d8 = _mm256_unpacklo_epi32(c8,c12);
    __m256i d12 = _mm256_unpackhi_epi32(c8,c12);
    __m256i d16 = _mm256_unpacklo_epi32(c16,c20);
    __m256i d20 = _mm256_unpackhi_epi32(c16,c20);
    __m256i d24 = _mm256_unpacklo_epi32(c24,c28);
    __m256i d28 = _mm256_unpackhi_epi32(c24,c28);
    __m256i e0 = _mm256_unpacklo_epi64(d0,d8);
    __m256i e8 = _mm256_unpackhi_epi64(d0,d8);
    __m256i e16 = _mm256_unpacklo_epi64(d16,d24);
    __m256i e24 = _mm256_unpackhi_epi64(d16,d24);
    __m256i e4 = _mm256_unpacklo_epi64(d4,d12);
    __m256i e12 = _mm256_unpackhi_epi64(d4,d12);
    __m256i e20 = _mm256_unpacklo_epi64(d20,d28);
    __m256i e28 = _mm256_unpackhi_epi64(d20,d28);
    __m256i f0 = _mm256_permute2x128_si256(e0,e16,0x20);
    __m256i f16 = _mm256_permute2x128_si256(e0,e16,0x31);
    __m256i f8 = _mm256_permute2x128_si256(e8,e24,0x20);
    __m256i f24 = _mm256_permute2x128_si256(e8,e24,0x31);
    __m256i f4 = _mm256_permute2x128_si256(e4,e20,0x20);
    __m256i f20 = _mm256_permute2x128_si256(e4,e20,0x31);
    __m256i f12 = _mm256_permute2x128_si256(e12,e28,0x20);
    __m256i f28 = _mm256_permute2x128_si256(e12,e28,0x31);
    x[r] = f0;
    x[r + 16] = f16;
    x[r + 1] = f8;
    x[r + 17] = f24;
    x[r + 2] = f4;
    x[r + 18] = f20;
    x[r + 3] = f12;
    x[r + 19] = f28;
  }
}

/* 48*16*int16 f inputs between -2295 and 2295 */
/* 24*32*int8 g inputs between -1 and 1 */
/* 96*16*int16 h outputs between -2295 and 2295 */
static void mult768_mix2_m256i(__m256i h[96],const __m256i f[48],const __m256i g[24])
{
  __m256i hkara[24][16];
  __m256i gkara[3][32];
#define fkara hkara
  int i;

  for (i = 6;i-- > 0;) {
    __m256i f0, f1, f2, f3, f4, f5, f6, f7;
    __m256i f01, f23, f45, f67;
    __m256i f02, f46, f04, f26, f0426;
    __m256i f13, f57, f15, f37, f1537;
    __m256i f0213, f4657, f04261537, f0415, f2637;

    f0 = _mm256_loadu_si256(&f[i + 0]);
    f1 = _mm256_loadu_si256(&f[i + 6]);
    f2 = _mm256_loadu_si256(&f[i + 12]);
    f3 = _mm256_loadu_si256(&f[i + 18]);
    f4 = _mm256_loadu_si256(&f[i + 24]);
    f5 = _mm256_loadu_si256(&f[i + 30]);
    f6 = _mm256_loadu_si256(&f[i + 36]);
    f7 = _mm256_loadu_si256(&f[i + 42]);
    f01 = squeezeadd16(f0,f1); fkara[i][8] = f01;
    f23 = squeezeadd16(f2,f3); fkara[i][9] = f23;
    f45 = squeezeadd16(f4,f5); fkara[i][10] = f45;
    f67 = squeezeadd16(f6,f7); fkara[i][11] = f67;

    fkara[i][0] = f0;
    fkara[i][2] = f2;
    fkara[i][4] = f4;
    fkara[i][6] = f6;

    f02 = squeezeadd16(f0,f2); fkara[i + 6][0] = f02;
    f04 = squeezeadd16(f0,f4); fkara[i + 6][6] = f04;
    f46 = squeezeadd16(f4,f6); fkara[i + 6][3] = f46;
    f26 = squeezeadd16(f2,f6); fkara[i + 6][8] = f26;

    fkara[i][1] = f1;
    fkara[i][3] = f3;
    fkara[i][5] = f5;
    fkara[i][7] = f7;

    f13 = squeezeadd16(f1,f3); fkara[i + 6][1] = f13;
    f15 = squeezeadd16(f1,f5); fkara[i + 6][7] = f15;
    f57 = squeezeadd16(f5,f7); fkara[i + 6][4] = f57;
    f37 = squeezeadd16(f3,f7); fkara[i + 6][9] = f37;

    f0426 = squeezeadd16(f04,f26); fkara[i + 6][12] = f0426;
    f1537 = squeezeadd16(f15,f37); fkara[i + 6][13] = f1537;
    f0213 = squeezeadd16(f02,f13); fkara[i + 6][2] = f0213;
    f4657 = squeezeadd16(f46,f57); fkara[i + 6][5] = f4657;
    f0415 = squeezeadd16(f04,f15); fkara[i + 6][10] = f0415;
    f2637 = squeezeadd16(f26,f37); fkara[i + 6][11] = f2637;
    f04261537 = squeezeadd16(f0426,f1537); fkara[i + 6][14] = f04261537;

    fkara[i][12] = v0;
    fkara[i][13] = v0;
    fkara[i][14] = v0;
    fkara[i][15] = v0;
    fkara[i + 6][15] = v0;
  }

  for (i = 3;i-- > 0;) {
    __m256i g0, g1, g2, g3, g4, g5, g6, g7;
    __m256i g01, g23, g45, g67;
    __m256i g02, g46, g04, g26, g0426;
    __m256i g13, g57, g15, g37, g1537;
    __m256i g0213, g4657, g04261537, g0415, g2637;

    g0 = _mm256_loadu_si256(&g[i + 0]);
    g1 = _mm256_loadu_si256(&g[i + 3]);
    g2 = _mm256_loadu_si256(&g[i + 6]);
    g3 = _mm256_loadu_si256(&g[i + 9]);
    g4 = _mm256_loadu_si256(&g[i + 12]);
    g5 = _mm256_loadu_si256(&g[i + 15]);
    g6 = _mm256_loadu_si256(&g[i + 18]);
    g7 = _mm256_loadu_si256(&g[i + 21]);
    g01 = _mm256_add_epi8(g0,g1); gkara[i][8] = g01;
    g23 = _mm256_add_epi8(g2,g3); gkara[i][9] = g23;
    g45 = _mm256_add_epi8(g4,g5); gkara[i][10] = g45;
    g67 = _mm256_add_epi8(g6,g7); gkara[i][11] = g67;

    gkara[i][0] = g0;
    gkara[i][2] = g2;
    gkara[i][4] = g4;
    gkara[i][6] = g6;

    g02 = _mm256_add_epi8(g0,g2); gkara[i][16] = g02;
    g04 = _mm256_add_epi8(g0,g4); gkara[i][22] = g04;
    g46 = _mm256_add_epi8(g4,g6); gkara[i][19] = g46;
    g26 = _mm256_add_epi8(g2,g6); gkara[i][24] = g26;

    gkara[i][1] = g1;
    gkara[i][3] = g3;
    gkara[i][5] = g5;
    gkara[i][7] = g7;

    g13 = _mm256_add_epi8(g1,g3); gkara[i][17] = g13;
    g15 = _mm256_add_epi8(g1,g5); gkara[i][23] = g15;
    g57 = _mm256_add_epi8(g5,g7); gkara[i][20] = g57;
    g37 = _mm256_add_epi8(g3,g7); gkara[i][25] = g37;

    g0426 = _mm256_add_epi8(g04,g26); gkara[i][28] = g0426;
    g1537 = _mm256_add_epi8(g15,g37); gkara[i][29] = g1537;
    g0213 = _mm256_add_epi8(g02,g13); gkara[i][18] = g0213;
    g4657 = _mm256_add_epi8(g46,g57); gkara[i][21] = g4657;
    g0415 = _mm256_add_epi8(g04,g15); gkara[i][26] = g0415;
    g2637 = _mm256_add_epi8(g26,g37); gkara[i][27] = g2637;
    g04261537 = _mm256_add_epi8(g0426,g1537); gkara[i][30] = g04261537;

    gkara[i][12] = v0;
    gkara[i][13] = v0;
    gkara[i][14] = v0;
    gkara[i][15] = v0;
    gkara[i][31] = v0;
  }

  for (i = 12;i-- > 0;)
    transpose16(fkara[i]);
  for (i = 3;i-- > 0;)
    transpose32(gkara[i]);

  mult96x16(hkara[12],fkara[6],(__m256i *) (1 + (__m128i *) gkara));
  mult96x16(hkara[0],fkara[0],gkara[0]);

  for (i = 24;i-- > 0;)
    transpose16(hkara[i]);

  for (i = 6;i-- > 0;) {
    __m256i h0,h1,h2,h3,h4,h5,h6,h7,h8,h9;
    __m256i h10,h11,h12,h13,h14,h15,h16,h17,h18,h19;
    __m256i h20,h21,h22,h23;
    __m256i h32,h33,h34,h35,h36,h37,h38,h39;
    __m256i h40,h41,h42,h43,h44,h45,h46,h47,h48,h49;
    __m256i h50,h51,h52,h53,h54,h55,h56,h57,h58,h59;
    __m256i h60,h61;
    __m256i c;

#define COMBINE(h0,h1,h2,h3,x0,x1) \
    c = _mm256_sub_epi16(h1,h2); \
    h1 = _mm256_sub_epi16(_mm256_add_epi16(c,x0),h0); \
    h2 = _mm256_sub_epi16(x1,_mm256_add_epi16(c,h3)); \
    h1 = squeeze(h1); \
    h2 = squeeze(h2);

    h56 = hkara[i + 12][12];
    h57 = hkara[i + 18][12];
    h58 = hkara[i + 12][13];
    h59 = hkara[i + 18][13];
    h60 = hkara[i + 12][14];
    h61 = hkara[i + 18][14];
    COMBINE(h56,h57,h58,h59,h60,h61)

    h44 = hkara[i + 12][6];
    h45 = hkara[i + 18][6];
    h46 = hkara[i + 12][7];
    h47 = hkara[i + 18][7];
    h52 = hkara[i + 12][10];
    h53 = hkara[i + 18][10];
    COMBINE(h44,h45,h46,h47,h52,h53)

    h48 = hkara[i + 12][8];
    h49 = hkara[i + 18][8];
    h50 = hkara[i + 12][9];
    h51 = hkara[i + 18][9];
    h54 = hkara[i + 12][11];
    h55 = hkara[i + 18][11];
    COMBINE(h48,h49,h50,h51,h54,h55)
    COMBINE(h44,h46,h48,h50,h56,h58)
    COMBINE(h45,h47,h49,h51,h57,h59)

    h0 = hkara[i][0];
    h1 = hkara[i + 6][0];
    h2 = hkara[i][1];
    h3 = hkara[i + 6][1];
    h16 = hkara[i][8];
    h17 = hkara[i + 6][8];
    COMBINE(h0,h1,h2,h3,h16,h17)

    h4 = hkara[i][2];
    h5 = hkara[i + 6][2];
    h6 = hkara[i][3];
    h7 = hkara[i + 6][3];
    h18 = hkara[i][9];
    h19 = hkara[i + 6][9];
    COMBINE(h4,h5,h6,h7,h18,h19)

    h32 = hkara[i + 12][0];
    h33 = hkara[i + 18][0];
    h34 = hkara[i + 12][1];
    h35 = hkara[i + 18][1];
    h36 = hkara[i + 12][2];
    h37 = hkara[i + 18][2];
    COMBINE(h32,h33,h34,h35,h36,h37)
    COMBINE(h1,h3,h5,h7,h33,h35)
    COMBINE(h0,h2,h4,h6,h32,h34)

    h8 = hkara[i][4];
    h9 = hkara[i + 6][4];
    h10 = hkara[i][5];
    h11 = hkara[i + 6][5];
    h20 = hkara[i][10];
    h21 = hkara[i + 6][10];
    COMBINE(h8,h9,h10,h11,h20,h21)

    h12 = hkara[i][6];
    h13 = hkara[i + 6][6];
    h14 = hkara[i][7];
    h15 = hkara[i + 6][7];
    h22 = hkara[i][11];
    h23 = hkara[i + 6][11];
    COMBINE(h12,h13,h14,h15,h22,h23)

    h38 = hkara[i + 12][3];
    h39 = hkara[i + 18][3];
    h40 = hkara[i + 12][4];
    h41 = hkara[i + 18][4];
    h42 = hkara[i + 12][5];
    h43 = hkara[i + 18][5];
    COMBINE(h38,h39,h40,h41,h42,h43)
    COMBINE(h8,h10,h12,h14,h38,h40)
    COMBINE(h9,h11,h13,h15,h39,h41)

    COMBINE(h0,h4,h8,h12,h44,h48)
    h0 = freeze(h0);
    h4 = freeze(h4);
    h8 = freeze(h8);
    h12 = freeze(h12);
    _mm256_storeu_si256(&h[i + 0],h0);
    _mm256_storeu_si256(&h[i + 24],h4);
    _mm256_storeu_si256(&h[i + 48],h8);
    _mm256_storeu_si256(&h[i + 72],h12);

    COMBINE(h1,h5,h9,h13,h45,h49)
    h1 = freeze(h1);
    h5 = freeze(h5);
    h9 = freeze(h9);
    h13 = freeze(h13);
    _mm256_storeu_si256(&h[i + 6],h1);
    _mm256_storeu_si256(&h[i + 30],h5);
    _mm256_storeu_si256(&h[i + 54],h9);
    _mm256_storeu_si256(&h[i + 78],h13);

    COMBINE(h2,h6,h10,h14,h46,h50)
    h2 = freeze(h2);
    h6 = freeze(h6);
    h10 = freeze(h10);
    h14 = freeze(h14);
    _mm256_storeu_si256(&h[i + 12],h2);
    _mm256_storeu_si256(&h[i + 36],h6);
    _mm256_storeu_si256(&h[i + 60],h10);
    _mm256_storeu_si256(&h[i + 84],h14);

    COMBINE(h3,h7,h11,h15,h47,h51)
    h3 = freeze(h3);
    h7 = freeze(h7);
    h11 = freeze(h11);
    h15 = freeze(h15);
    _mm256_storeu_si256(&h[i + 18],h3);
    _mm256_storeu_si256(&h[i + 42],h7);
    _mm256_storeu_si256(&h[i + 66],h11);
    _mm256_storeu_si256(&h[i + 90],h15);
  }
}

#define p 761

/* 761 f inputs between -2295 and 2295 */
/* 761 g inputs between -1 and 1 */
/* 761 h outputs between -2295 and 2295 */
void rq_mult(modq *h,const modq *f,const small *g)
{
  __m256i fgvec[96];
  modq *fg;
  int i;

  mult768_mix2_m256i(fgvec,(__m256i *) f,(__m256i *) g);
  fg = (modq *) fgvec;

  h[0] = modq_freeze(fg[0] + fg[p]);
  for (i = 1;i < 9;++i)
    h[i] = modq_freeze(fg[i] + fg[i + p - 1] + fg[i + p]);
  for (i = 9;i < 761;i += 16) {
    __m256i fgi = _mm256_loadu_si256((__m256i *) &fg[i]);
    __m256i fgip = _mm256_loadu_si256((__m256i *) &fg[i + p]);
    __m256i fgip1 = _mm256_loadu_si256((__m256i *) &fg[i + p - 1]);
    __m256i x = _mm256_add_epi16(fgi,_mm256_add_epi16(fgip,fgip1));
    x = freeze(squeeze(x));
    _mm256_storeu_si256((__m256i *) &h[i],x);
  }
  for (i = 761;i < 768;++i)
    h[i] = 0;
}
