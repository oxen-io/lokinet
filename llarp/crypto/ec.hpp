#ifndef LLARP_CRYPTO_EC_HPP
#define LLARP_CRYPTO_EC_HPP

#include "types.hpp"

extern "C"
{
#include <sodium/private/ed25519_ref10.h>
}

namespace llarp
{
  namespace sodium
  {
    void
    sc25519_reduce32(byte_t *s);
    /*
      Input:
      a[0]+256*a[1]+...+256^31*a[31] = a
      b[0]+256*b[1]+...+256^31*b[31] = b
      c[0]+256*c[1]+...+256^31*c[31] = c

      Output:
      s[0]+256*s[1]+...+256^31*s[31] = (c-ab) mod l
      where l = 2^252 + 27742317777372353535851937790883648493.
    */
    void
    sc25519_mulsub(byte_t *s, const byte_t *a, const byte_t *b,
                   const byte_t *c);

    void
    sc25519_sub(byte_t *, const byte_t *, const byte_t *);

    int
    ge25519_frombytes_vartime(ge25519_p3 *, const byte_t *);

    int
    sc25519_check(const byte_t *);

    void
    ge25519_double_scalarmult_base_vartime(ge25519_p2 *, const byte_t *,
                                           const ge25519_p3 *, const byte_t *);
  }  // namespace sodium
}  // namespace llarp

#endif
