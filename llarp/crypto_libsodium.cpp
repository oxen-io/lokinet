#include <assert.h>
#include <llarp/crypto.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <llarp/crypto.hpp>
#include "mem.hpp"

extern "C"
{
  extern int
  sodium_init(void);
}

namespace llarp
{
  namespace sodium
  {
    static bool
    xchacha20(llarp_buffer_t buff, const byte_t *k, const byte_t *n)
    {
      return crypto_stream_xchacha20_xor(buff.base, buff.base, buff.sz, n, k)
          == 0;
    }

    static bool
    dh(uint8_t *out, const uint8_t *client_pk, const uint8_t *server_pk,
       const uint8_t *themPub, const uint8_t *usSec)
    {
      llarp::SharedSecret shared;
      crypto_generichash_state h;
      const size_t outsz = SHAREDKEYSIZE;

      if(crypto_scalarmult_curve25519(shared, usSec, themPub))
        return false;
      crypto_generichash_blake2b_init(&h, nullptr, 0U, outsz);
      crypto_generichash_blake2b_update(&h, client_pk, 32);
      crypto_generichash_blake2b_update(&h, server_pk, 32);
      crypto_generichash_blake2b_update(&h, shared, 32);
      crypto_generichash_blake2b_final(&h, out, outsz);
      return true;
    }

    static bool
    dh_client(uint8_t *shared, const uint8_t *pk, const uint8_t *sk,
              const uint8_t *n)
    {
      llarp::SharedSecret dh_result;

      if(dh(dh_result, llarp::seckey_topublic(sk), pk, pk, sk))
      {
        return crypto_generichash_blake2b(shared, 32, n, 32, dh_result, 32)
            != -1;
      }
      llarp::LogWarn("crypto::dh_client - dh failed");
      return false;
    }

    static bool
    dh_server(uint8_t *shared, const uint8_t *pk, const uint8_t *sk,
              const uint8_t *n)
    {
      llarp::SharedSecret dh_result;
      if(dh(dh_result, pk, llarp::seckey_topublic(sk), pk, sk))
      {
        return crypto_generichash_blake2b(shared, 32, n, 32, dh_result, 32)
            != -1;
      }
      llarp::LogWarn("crypto::dh_server - dh failed");
      return false;
    }

    static bool
    hash(uint8_t *result, llarp_buffer_t buff)
    {
      return crypto_generichash_blake2b(result, HASHSIZE, buff.base, buff.sz,
                                        nullptr, 0)
          != -1;
    }

    static bool
    shorthash(uint8_t *result, llarp_buffer_t buff)
    {
      return crypto_generichash_blake2b(result, SHORTHASHSIZE, buff.base,
                                        buff.sz, nullptr, 0)
          != -1;
    }

    static bool
    hmac(uint8_t *result, llarp_buffer_t buff, const uint8_t *secret)
    {
      return crypto_generichash_blake2b(result, HMACSIZE, buff.base, buff.sz,
                                        secret, HMACSECSIZE)
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

  const byte_t *
  seckey_topublic(const byte_t *sec)
  {
    return sec + 32;
  }

  namespace pq
  {
    bool
    encrypt(byte_t *ciphertext, byte_t *sharedkey, const byte_t *pubkey)
    {
      return crypto_kem_enc(ciphertext, sharedkey, pubkey) != -1;
    }
    bool
    decrypt(const byte_t *ciphertext, byte_t *sharedkey,
            const byte_t *secretkey)
    {
      return crypto_kem_dec(sharedkey, ciphertext, secretkey) != -1;
    }

    void
    keygen(byte_t *keypair)
    {
      crypto_kem_keypair(keypair + PQ_SECRETKEYSIZE, keypair);
    }
  }  // namespace pq

  const byte_t *
  pq_keypair_to_public(const byte_t *k)
  {
    return k + PQ_SECRETKEYSIZE;
  }

  const byte_t *
  pq_keypair_to_secret(const byte_t *k)
  {
    return k;
  }

}  // namespace llarp

void
llarp_crypto_init(struct llarp_crypto *c)
{
  assert(sodium_init() != -1);
  char *avx2 = getenv("AVX2_FORCE_DISABLE");
  if(avx2 && std::string(avx2) == "1")
    ntru_init(1);
  else
    ntru_init(0);
  c->xchacha20           = llarp::sodium::xchacha20;
  c->dh_client           = llarp::sodium::dh_client;
  c->dh_server           = llarp::sodium::dh_server;
  c->transport_dh_client = llarp::sodium::dh_client;
  c->transport_dh_server = llarp::sodium::dh_server;
  c->hash                = llarp::sodium::hash;
  c->shorthash           = llarp::sodium::shorthash;
  c->hmac                = llarp::sodium::hmac;
  c->sign                = llarp::sodium::sign;
  c->verify              = llarp::sodium::verify;
  c->randomize           = llarp::sodium::randomize;
  c->randbytes           = llarp::sodium::randbytes;
  c->identity_keygen     = llarp::sodium::sigkeygen;
  c->encryption_keygen   = llarp::sodium::enckeygen;
  c->pqe_encrypt         = llarp::pq::encrypt;
  c->pqe_decrypt         = llarp::pq::decrypt;
  c->pqe_keygen          = llarp::pq::keygen;
  int seed;
  c->randbytes(&seed, sizeof(seed));
  srand(seed);
}

uint64_t
llarp_randint()
{
  uint64_t i;
  randombytes((byte_t *)&i, sizeof(i));
  return i;
}
