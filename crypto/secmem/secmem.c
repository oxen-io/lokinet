/* [Rtl]SecureZeroMemory is an inline procedure in the windows headers */
#ifdef _WIN32
#include <windows.h>
#endif
#include <sodium/utils.h>

void
sodium_memzero(void *const pnt, const size_t len)
{
#ifdef _WIN32
  SecureZeroMemory(pnt, len);
#elif defined(HAVE_EXPLICIT_BZERO)
  explicit_bzero(pnt, len);
#else
  volatile unsigned char *volatile pnt_ = (volatile unsigned char *volatile)pnt;
  size_t i                              = (size_t)0U;

  while(i < len)
  {
    pnt_[i++] = 0U;
  }
#endif
}

int
sodium_is_zero(const unsigned char *n, const size_t nlen)
{
  size_t i;
  volatile unsigned char d = 0U;

  for(i = 0U; i < nlen; i++)
  {
    d |= n[i];
  }
  return 1 & ((d - 1) >> 8);
}

int
sodium_memcmp(const void *const b1_, const void *const b2_, size_t len)
{
  const volatile unsigned char *volatile b1 =
      (const volatile unsigned char *volatile)b1_;
  const volatile unsigned char *volatile b2 =
      (const volatile unsigned char *volatile)b2_;
  size_t i;
  volatile unsigned char d = 0U;

  for(i = 0U; i < len; i++)
  {
    d |= b1[i] ^ b2[i];
  }
  return (1 & ((d - 1) >> 8)) - 1;
}
