#ifdef __AVX2__
#include "int32_sort.h"
#include <immintrin.h>

typedef crypto_int32 int32;

static inline void
minmax(int32 *x, int32 *y)
{
  __asm__("movl (%0),%%eax;movl (%1),%%ebx;cmpl %%ebx,%%eax;mov %%eax,%%edx;cmovg "
      "%%ebx,%%eax;cmovg %%edx,%%ebx;movl %%eax,(%0);movl %%ebx,(%1)"
      :
      : "r"(x), "r"(y)
      : "%eax", "%ebx", "%edx");
}

/* sort x0,x2; sort x1,x3; ... sort x13, x15 */
static inline void
minmax02through1315(int32 *x)
{
  __m256i a = _mm256_loadu_si256((__m256i *)x);
  __m256i b = _mm256_loadu_si256((__m256i *)(x + 8));
  __m256i c = _mm256_unpacklo_epi64(a, b); /* a01b01a45b45 */
  __m256i d = _mm256_unpackhi_epi64(a, b); /* a23b23a67b67 */
  __m256i g = _mm256_min_epi32(c, d);
  __m256i h = _mm256_max_epi32(c, d);
  a         = _mm256_unpacklo_epi64(g, h);
  b         = _mm256_unpackhi_epi64(g, h);
  _mm256_storeu_si256((__m256i *)x, a);
  _mm256_storeu_si256((__m256i *)(x + 8), b);
}

/* sort x0,x2; sort x1,x3; sort x4,x6; sort x5,x7 */
static inline void
minmax02134657(int32 *x)
{
  __m256i a   = _mm256_loadu_si256((__m256i *)x);
  __m256i b   = _mm256_shuffle_epi32(a, 0x4e);
  __m256i c   = _mm256_cmpgt_epi32(a, b);
  c           = _mm256_shuffle_epi32(c, 0x44);
  __m256i abc = c & (a ^ b);
  a ^= abc;
  _mm256_storeu_si256((__m256i *)x, a);
}

static void
multiminmax2plus2(int32 *x, int n)
{
  while(n >= 16)
  {
    minmax02through1315(x);
    n -= 16;
    x += 16;
  }
  if(n >= 8)
  {
    minmax02134657(x);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
  {
    minmax(x, x + 2);
    minmax(x + 1, x + 3);
    n -= 4;
    x += 4;
  }
  if(n > 0)
  {
    minmax(x, x + 2);
    if(n > 1)
      minmax(x + 1, x + 3);
  }
}

static void
multiminmax2plus6(int32 *x, int n)
{
  while(n >= 4)
  {
    minmax(x, x + 6);
    minmax(x + 1, x + 7);
    n -= 4;
    x += 4;
  }
  if(n > 0)
  {
    minmax(x, x + 6);
    if(n > 1)
      minmax(x + 1, x + 7);
  }
}

static void
multiminmax2plus14(int32 *x, int n)
{
  while(n >= 8)
  {
    minmax(x, x + 14);
    minmax(x + 1, x + 15);
    minmax(x + 4, x + 18);
    minmax(x + 5, x + 19);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
  {
    minmax(x, x + 14);
    minmax(x + 1, x + 15);
    n -= 4;
    x += 4;
  }
  if(n > 0)
  {
    minmax(x, x + 14);
    if(n > 1)
      minmax(x + 1, x + 15);
  }
}

/* sort x[i],y[i] for i in 0,1,4,5,8,9,12,13 */
/* all of x0...x15 and y0...y15 must exist; no aliasing */
static inline void
minmax0145891213(int32 *x, int32 *y)
{
  __m256i a01234567       = _mm256_loadu_si256((__m256i *)x);
  __m256i a89101112131415 = _mm256_loadu_si256((__m256i *)(x + 8));
  __m256i b01234567       = _mm256_loadu_si256((__m256i *)y);
  __m256i b89101112131415 = _mm256_loadu_si256((__m256i *)(y + 8));

  __m256i a0189451213 = _mm256_unpacklo_epi64(a01234567, a89101112131415);
  __m256i b0189451213 = _mm256_unpacklo_epi64(b01234567, b89101112131415);
  __m256i c0189451213 = _mm256_min_epi32(a0189451213, b0189451213);
  __m256i d0189451213 = _mm256_max_epi32(a0189451213, b0189451213);

  __m256i c01234567       = _mm256_blend_epi32(a01234567, c0189451213, 0x33);
  __m256i d01234567       = _mm256_blend_epi32(b01234567, d0189451213, 0x33);
  __m256i c89101112131415 = _mm256_unpackhi_epi64(c0189451213, a89101112131415);
  __m256i d89101112131415 = _mm256_unpackhi_epi64(d0189451213, b89101112131415);

  _mm256_storeu_si256((__m256i *)x, c01234567);
  _mm256_storeu_si256((__m256i *)(x + 8), c89101112131415);
  _mm256_storeu_si256((__m256i *)y, d01234567);
  _mm256_storeu_si256((__m256i *)(y + 8), d89101112131415);
}

/* offset >= 30 */
static void
multiminmax2plusmore(int32 *x, int n, int offset)
{
  while(n >= 16)
  {
    minmax0145891213(x, x + offset);
    n -= 16;
    x += 16;
  }
  if(n >= 8)
  {
    minmax(x, x + offset);
    minmax(x + 1, x + 1 + offset);
    minmax(x + 4, x + 4 + offset);
    minmax(x + 5, x + 5 + offset);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
  {
    minmax(x, x + offset);
    minmax(x + 1, x + 1 + offset);
    n -= 4;
    x += 4;
  }
  if(n > 0)
  {
    minmax(x, x + offset);
    if(n > 1)
      minmax(x + 1, x + 1 + offset);
  }
}

/* sort x0,x1; ... sort x14, x15 */
static inline void
minmax01through1415(int32 *x)
{
  __m256i a = _mm256_loadu_si256((__m256i *)x);
  __m256i b = _mm256_loadu_si256((__m256i *)(x + 8));
  __m256i c = _mm256_unpacklo_epi32(a, b); /* ab0ab1ab4ab5 */
  __m256i d = _mm256_unpackhi_epi32(a, b); /* ab2ab3ab6ab7 */
  __m256i e = _mm256_unpacklo_epi32(c, d); /* a02b02a46b46 */
  __m256i f = _mm256_unpackhi_epi32(c, d); /* a13b13a57b57 */
  __m256i g = _mm256_min_epi32(e, f);      /* a02b02a46b46 */
  __m256i h = _mm256_max_epi32(e, f);      /* a13b13a57b57 */
  a         = _mm256_unpacklo_epi32(g, h);
  b         = _mm256_unpackhi_epi32(g, h);
  _mm256_storeu_si256((__m256i *)x, a);
  _mm256_storeu_si256((__m256i *)(x + 8), b);
}

/* sort x0,x1; sort x2,x3; sort x4,x5; sort x6,x7 */
static inline void
minmax01234567(int32 *x)
{
  __m256i a   = _mm256_loadu_si256((__m256i *)x);
  __m256i b   = _mm256_shuffle_epi32(a, 0xb1);
  __m256i c   = _mm256_cmpgt_epi32(a, b);
  c           = _mm256_shuffle_epi32(c, 0xa0);
  __m256i abc = c & (a ^ b);
  a ^= abc;
  _mm256_storeu_si256((__m256i *)x, a);
}

static void
multiminmax1plus1(int32 *x, int n)
{
  while(n >= 16)
  {
    minmax01through1415(x);
    n -= 16;
    x += 16;
  }
  if(n >= 8)
  {
    minmax01234567(x);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
  {
    minmax(x, x + 1);
    minmax(x + 2, x + 3);
    n -= 4;
    x += 4;
  }
  if(n >= 2)
  {
    minmax(x, x + 1);
    n -= 2;
    x += 2;
  }
  if(n > 0)
    minmax(x, x + 1);
}

static void
multiminmax1(int32 *x, int n, int offset)
{
  while(n >= 16)
  {
    minmax(x, x + offset);
    minmax(x + 2, x + 2 + offset);
    minmax(x + 4, x + 4 + offset);
    minmax(x + 6, x + 6 + offset);
    minmax(x + 8, x + 8 + offset);
    minmax(x + 10, x + 10 + offset);
    minmax(x + 12, x + 12 + offset);
    minmax(x + 14, x + 14 + offset);
    n -= 16;
    x += 16;
  }
  if(n >= 8)
  {
    minmax(x, x + offset);
    minmax(x + 2, x + 2 + offset);
    minmax(x + 4, x + 4 + offset);
    minmax(x + 6, x + 6 + offset);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
  {
    minmax(x, x + offset);
    minmax(x + 2, x + 2 + offset);
    n -= 4;
    x += 4;
  }
  if(n >= 2)
  {
    minmax(x, x + offset);
    n -= 2;
    x += 2;
  }
  if(n > 0)
    minmax(x, x + offset);
}

/* sort x[i],y[i] for i in 0,2,4,6,8,10,12,14 */
/* all of x0...x15 and y0...y15 must exist; no aliasing */
static inline void
minmax02468101214(int32 *x, int32 *y)
{
  __m256i a01234567       = _mm256_loadu_si256((__m256i *)x);
  __m256i a89101112131415 = _mm256_loadu_si256((__m256i *)(x + 8));
  __m256i b01234567       = _mm256_loadu_si256((__m256i *)y);
  __m256i b89101112131415 = _mm256_loadu_si256((__m256i *)(y + 8));

  __m256i a0819412513   = _mm256_unpacklo_epi32(a01234567, a89101112131415);
  __m256i a210311614715 = _mm256_unpackhi_epi32(a01234567, a89101112131415);
  __m256i a02810461214  = _mm256_unpacklo_epi32(a0819412513, a210311614715);
  __m256i a13911571315  = _mm256_unpackhi_epi32(a0819412513, a210311614715);

  __m256i b0819412513   = _mm256_unpacklo_epi32(b01234567, b89101112131415);
  __m256i b210311614715 = _mm256_unpackhi_epi32(b01234567, b89101112131415);
  __m256i b02810461214  = _mm256_unpacklo_epi32(b0819412513, b210311614715);
  __m256i b13911571315  = _mm256_unpackhi_epi32(b0819412513, b210311614715);

  __m256i c02810461214 = _mm256_min_epi32(a02810461214, b02810461214);
  __m256i d02810461214 = _mm256_max_epi32(a02810461214, b02810461214);

  __m256i c01234567       = _mm256_unpacklo_epi32(c02810461214, a13911571315);
  __m256i c89101112131415 = _mm256_unpackhi_epi32(c02810461214, a13911571315);
  __m256i d01234567       = _mm256_unpacklo_epi32(d02810461214, b13911571315);
  __m256i d89101112131415 = _mm256_unpackhi_epi32(d02810461214, b13911571315);

  _mm256_storeu_si256((__m256i *)x, c01234567);
  _mm256_storeu_si256((__m256i *)(x + 8), c89101112131415);
  _mm256_storeu_si256((__m256i *)y, d01234567);
  _mm256_storeu_si256((__m256i *)(y + 8), d89101112131415);
}

/* assumes offset >= 31 */
static void
multiminmax1plusmore(int32 *x, int n, int offset)
{
  while(n >= 16)
  {
    minmax02468101214(x, x + offset);
    n -= 16;
    x += 16;
  }
  if(n >= 8)
  {
    minmax(x, x + offset);
    minmax(x + 2, x + 2 + offset);
    minmax(x + 4, x + 4 + offset);
    minmax(x + 6, x + 6 + offset);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
  {
    minmax(x, x + offset);
    minmax(x + 2, x + 2 + offset);
    n -= 4;
    x += 4;
  }
  if(n >= 2)
  {
    minmax(x, x + offset);
    n -= 2;
    x += 2;
  }
  if(n > 0)
    minmax(x, x + offset);
}

/* sort x0,y0; sort x1,y1; ...; sort x7,y7 */
static inline void
minmax8(int32 *x, int32 *y)
{
  __m256i a = _mm256_loadu_si256((__m256i *)x);
  __m256i b = _mm256_loadu_si256((__m256i *)y);
  _mm256_storeu_si256((__m256i *)x, _mm256_min_epi32(a, b));
  _mm256_storeu_si256((__m256i *)y, _mm256_max_epi32(a, b));
}

/* assumes p >= 8; implies offset >= 8 */
static void
multiminmax_atleast8(int p, int32 *x, int n, int offset)
{
  int i;
  while(n >= 2 * p)
  {
    for(i = 0; i < p; i += 8)
      minmax8(x + i, x + i + offset);
    n -= 2 * p;
    x += 2 * p;
  }
  for(i = 0; i + 8 <= n; i += 8)
  {
    if(i & p)
      return;
    minmax8(x + i, x + i + offset);
  }
  for(; i < n; ++i)
  {
    if(i & p)
      return;
    minmax(x + i, x + i + offset);
  }
}

/* sort x0,y0; sort x1,y1; sort x2,y2; sort x3,y3 */
static inline void
minmax4(int32 *x, int32 *y)
{
  __m128i a = _mm_loadu_si128((__m128i *)x);
  __m128i b = _mm_loadu_si128((__m128i *)y);
  _mm_storeu_si128((__m128i *)x, _mm_min_epi32(a, b));
  _mm_storeu_si128((__m128i *)y, _mm_max_epi32(a, b));
}

static void
multiminmax4(int32 *x, int n, int offset)
{
  int i;
  while(n >= 8)
  {
    minmax4(x, x + offset);
    n -= 8;
    x += 8;
  }
  if(n >= 4)
    minmax4(x, x + offset);
  else
    for(i = 0; i < n; ++i)
      minmax(x + i, x + i + offset);
}

void
int32_sort(int32 *x, int n)
{
  int top, p, q;

  if(n < 2)
    return;
  top = 1;
  while(top < n - top)
    top += top;

  for(p = top; p >= 8; p >>= 1)
  {
    multiminmax_atleast8(p, x, n - p, p);
    for(q = top; q > p; q >>= 1)
      multiminmax_atleast8(p, x + p, n - q, q - p);
  }
  if(p >= 4)
  {
    multiminmax4(x, n - 4, 4);
    for(q = top; q > 4; q >>= 1)
      multiminmax4(x + 4, n - q, q - 4);
  }
  if(p >= 2)
  {
    multiminmax2plus2(x, n - 2);
    for(q = top; q >= 32; q >>= 1)
      multiminmax2plusmore(x + 2, n - q, q - 2);
    if(q >= 16)
      multiminmax2plus14(x + 2, n - 16);
    if(q >= 8)
      multiminmax2plus6(x + 2, n - 8);
    if(q >= 4)
      multiminmax2plus2(x + 2, n - 4);
  }
  multiminmax1plus1(x, n - 1);
  for(q = top; q >= 32; q >>= 1)
    multiminmax1plusmore(x + 1, n - q, q - 1);
  if(q >= 16)
    multiminmax1(x + 1, n - 16, 15);
  if(q >= 8)
    multiminmax1(x + 1, n - 8, 7);
  if(q >= 4)
    multiminmax1(x + 1, n - 4, 3);
  if(q >= 2)
    multiminmax1plus1(x + 1, n - 2);
}
#endif
