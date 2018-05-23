#include <assert.h>
#include <llarp/crypto.h>
#include <sodium.h>
#include "mem.hpp"

namespace llarp
{
  namespace sodium
  {
    static bool
    xchacha20(llarp_buffer_t buff, llarp_sharedkey_t k, llarp_nonce_t n)
    {
      return crypto_stream_xchacha20_xor(buff.base, buff.base, buff.sz, n, k)
          == 0;
    }

    static bool
    dh(uint8_t *out, uint8_t *client_pk, uint8_t *server_pk, uint8_t *pubkey,
       uint8_t *secret)
    {
      llarp_sharedkey_t shared;
      crypto_generichash_state h;
      const size_t outsz = SHAREDKEYSIZE;

      if(crypto_scalarmult_curve25519(shared, secret, pubkey))
        return false;
      crypto_generichash_init(&h, NULL, 0U, outsz);
      crypto_generichash_update(&h, client_pk, 32);
      crypto_generichash_update(&h, server_pk, 32);
      crypto_generichash_update(&h, shared, 32);
      crypto_generichash_final(&h, out, outsz);
      return true;
    }

    static bool
    dh_client(llarp_sharedkey_t *shared, llarp_pubkey_t pk,
              llarp_tunnel_nonce_t n, llarp_seckey_t sk)
    {
      if(dh(*shared, llarp_seckey_topublic(sk), pk, pk, sk))
      {
        return crypto_generichash(*shared, SHAREDKEYSIZE, *shared,
                                  SHAREDKEYSIZE, n, TUNNONCESIZE)
            != -1;
      }
      return false;
    }

    static bool
    dh_server(llarp_sharedkey_t *shared, llarp_pubkey_t pk,
              llarp_tunnel_nonce_t n, llarp_seckey_t sk)
    {
      if(dh(*shared, pk, llarp_seckey_topublic(sk), pk, sk))
      {
        return crypto_generichash(*shared, SHAREDKEYSIZE, *shared,
                                  SHAREDKEYSIZE, n, TUNNONCESIZE)
            != -1;
      }
      return false;
    }

    static bool
    transport_dh_client(uint8_t *shared, uint8_t *pk, uint8_t *sk, uint8_t *n)
    {
      llarp_sharedkey_t dh_result;
      if(dh(dh_result, llarp_seckey_topublic(sk), pk, pk, sk))
      {
        return crypto_generichash(shared, 32, n, 32, dh_result, 32) != -1;
      }
      return false;
    }

    static bool
    transport_dh_server(uint8_t *shared, uint8_t *pk, uint8_t *sk, uint8_t *n)
    {
      llarp_sharedkey_t dh_result;
      if(dh(dh_result, pk, llarp_seckey_topublic(sk), pk, sk))
      {
        return crypto_generichash(shared, 32, n, 32, dh_result, 32) != -1;
      }
      return false;
    }

    static bool
    hash(uint8_t *result, llarp_buffer_t buff)
    {
      return crypto_generichash(result, HASHSIZE, buff.base, buff.sz, nullptr,
                                0)
          != -1;
    }

    static bool
    shorthash(uint8_t *result, llarp_buffer_t buff)
    {
      return crypto_generichash(result, SHORTHASHSIZE, buff.base, buff.sz,
                                nullptr, 0)
          != -1;
    }

    static bool
    hmac(uint8_t *result, llarp_buffer_t buff, const uint8_t *secret)
    {
      return crypto_generichash(result, HMACSIZE, buff.base, buff.sz, secret,
                                HMACSECSIZE)
          != -1;
    }

    static bool
    sign(uint8_t *result, const uint8_t *secret, llarp_buffer_t buff)
    {
      return crypto_sign_detached(result, nullptr, buff.base, buff.sz, secret)
          != -1;
    }

    static bool
    verify(const uint8_t *pub, llarp_buffer_t buff, const uint8_t *sig)
    {
      return crypto_sign_verify_detached(sig, buff.base, buff.sz, pub) != -1;
    }

    static void
    randomize(llarp_buffer_t buff)
    {
      randombytes((unsigned char *)buff.base, buff.sz);
    }

    static inline void
    randbytes(void *ptr, size_t sz)
    {
      randombytes((unsigned char *)ptr, sz);
    }

    static void
    sigkeygen(uint8_t *keys)
    {
      crypto_sign_keypair(keys + 32, keys);
    }

    static void
    enckeygen(uint8_t *keys)
    {
      randombytes(keys, 32);
      crypto_scalarmult_curve25519_base(keys + 32, keys);
    }
  }  // namespace sodium
}  // namespace llarp

extern "C" {

uint8_t *
llarp_seckey_topublic(uint8_t *secret)
{
  return secret + 32;
}

void
llarp_crypto_libsodium_init(struct llarp_crypto *c)
{
  assert(sodium_init() != -1);
  c->xchacha20           = llarp::sodium::xchacha20;
  c->dh_client           = llarp::sodium::dh_client;
  c->dh_server           = llarp::sodium::dh_server;
  c->transport_dh_client = llarp::sodium::transport_dh_client;
  c->transport_dh_server = llarp::sodium::transport_dh_server;
  c->hash                = llarp::sodium::hash;
  c->shorthash           = llarp::sodium::shorthash;
  c->hmac                = llarp::sodium::hmac;
  c->sign                = llarp::sodium::sign;
  c->verify              = llarp::sodium::verify;
  c->randomize           = llarp::sodium::randomize;
  c->randbytes           = llarp::sodium::randbytes;
  c->identity_keygen     = llarp::sodium::sigkeygen;
  c->encryption_keygen   = llarp::sodium::enckeygen;
  int seed;
  c->randbytes(&seed, sizeof(seed));
  srand(seed);
}
}
