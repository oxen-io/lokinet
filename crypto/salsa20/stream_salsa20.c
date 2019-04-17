#include <sodium/crypto_stream_salsa20.h>
#include <sodium/private/common.h>
#include <sodium/private/implementations.h>
#include <sodium/randombytes.h>
#include <sodium/runtime.h>
#include "stream_salsa20.h"
#include "xmm6/salsa20_xmm6.h"
#include "ref/salsa20_ref.h"
#include "xmm6int/salsa20_xmm6int-sse2.h"
#include "xmm6int/salsa20_xmm6int-avx2.h"

static crypto_stream_salsa20_implementation *implementation = NULL;

size_t
crypto_stream_salsa20_keybytes(void)
{
  return crypto_stream_salsa20_KEYBYTES;
}

size_t
crypto_stream_salsa20_noncebytes(void)
{
  return crypto_stream_salsa20_NONCEBYTES;
}

size_t
crypto_stream_salsa20_messagebytes_max(void)
{
  return crypto_stream_salsa20_MESSAGEBYTES_MAX;
}

int
crypto_stream_salsa20(unsigned char *c, unsigned long long clen,
                      const unsigned char *n, const unsigned char *k)
{
  _crypto_stream_salsa20_pick_best_implementation();
  return implementation->stream(c, clen, n, k);
}

int
crypto_stream_salsa20_xor_ic(unsigned char *c, const unsigned char *m,
                             unsigned long long mlen, const unsigned char *n,
                             uint64_t ic, const unsigned char *k)
{
  _crypto_stream_salsa20_pick_best_implementation();
  return implementation->stream_xor_ic(c, m, mlen, n, ic, k);
}

int
crypto_stream_salsa20_xor(unsigned char *c, const unsigned char *m,
                          unsigned long long mlen, const unsigned char *n,
                          const unsigned char *k)
{
  _crypto_stream_salsa20_pick_best_implementation();
  return implementation->stream_xor_ic(c, m, mlen, n, 0U, k);
}

void
crypto_stream_salsa20_keygen(unsigned char k[crypto_stream_salsa20_KEYBYTES])
{
  randombytes_buf(k, crypto_stream_salsa20_KEYBYTES);
}

int
_crypto_stream_salsa20_pick_best_implementation(void)
{
  if(implementation)
    return 0;
#if defined(iOS)
  if(implementation == NULL)
    implementation = &crypto_stream_salsa20_ref_implementation;
  return 0; /* LCOV_EXCL_LINE */
#endif
#if __AVX2__
  if(sodium_runtime_has_avx2())
  {
    implementation = &crypto_stream_salsa20_xmm6int_avx2_implementation;
    return 0;
  }
#endif
#if __SSE2__
  if(sodium_runtime_has_sse2())
  {
    implementation = &crypto_stream_salsa20_xmm6int_sse2_implementation;
    return 0;
  }
#endif
  if(implementation == NULL)
    implementation = &crypto_stream_salsa20_ref_implementation;
  return 0; /* LCOV_EXCL_LINE */
}
