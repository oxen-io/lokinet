#include <assert.h>
#include <crypto.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <crypto.hpp>
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
    xchacha20_alt(llarp_buffer_t out, llarp_buffer_t in, const byte_t *k,
                  const byte_t *n)
    {
      if(in.sz > out.sz)
        return false;
      return crypto_stream_xchacha20_xor(out.base, in.base, in.sz, n, k) == 0;
    }

    static bool
    dh(llarp::SharedSecret &out, const PubKey &client_pk,
       const uint8_t *server_pk, const uint8_t *themPub, const uint8_t *usSec)
    {
      llarp::SharedSecret shared;
      crypto_generichash_state h;
      const size_t outsz = SHAREDKEYSIZE;

      if(crypto_scalarmult_curve25519(shared.as_array().data(), usSec, themPub))
        return false;
      crypto_generichash_blake2b_init(&h, nullptr, 0U, outsz);
      crypto_generichash_blake2b_update(&h, client_pk.as_array().data(), 32);
      crypto_generichash_blake2b_update(&h, server_pk, 32);
      crypto_generichash_blake2b_update(&h, shared, 32);
      crypto_generichash_blake2b_final(&h, out.as_array().data(), outsz);
      return true;
    }

    static bool
    dh_client(llarp::SharedSecret &shared, const PubKey &pk, const uint8_t *sk,
              const uint8_t *n)
    {
      llarp::SharedSecret dh_result;

      if(dh(dh_result, llarp::seckey_topublic(sk), pk, pk, sk))
      {
        return crypto_generichash_blake2b(shared.as_array().data(), 32, n, 32,
                                          dh_result, 32)
            != -1;
      }
      llarp::LogWarn("crypto::dh_client - dh failed");
      return false;
    }

    static bool
    dh_server(llarp::SharedSecret &shared, const uint8_t *pk, const uint8_t *sk,
              const uint8_t *n)
    {
      llarp::SharedSecret dh_result;
      if(dh(dh_result, pk, llarp::seckey_topublic(sk), pk, sk))
      {
        return crypto_generichash_blake2b(shared.as_array().data(), 32, n, 32,
                                          dh_result, 32)
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
    shorthash(ShortHash &result, llarp_buffer_t buff)
    {
      return crypto_generichash_blake2b(result.as_array().data(),
                                        ShortHash::SIZE, buff.base, buff.sz,
                                        nullptr, 0)
          != -1;
    }

    static bool
    hmac(byte_t *result, llarp_buffer_t buff, const SharedSecret &secret)
    {
      return crypto_generichash_blake2b(result, HMACSIZE, buff.base, buff.sz,
                                        secret.as_array().data(), HMACSECSIZE)
          != -1;
    }

    static bool
    sign(Signature &result, const SecretKey &secret, llarp_buffer_t buff)
    {
      return crypto_sign_detached(result.as_array().begin(), nullptr, buff.base,
                                  buff.sz, secret.as_array().begin())
          != -1;
    }

    static bool
    verify(const PubKey &pub, llarp_buffer_t buff, const uint8_t *sig)
    {
      return crypto_sign_verify_detached(sig, buff.base, buff.sz,
                                         pub.as_array().data())
          != -1;
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
    sigkeygen(llarp::SecretKey &keys)
    {
      auto d = keys.as_array().data();
      crypto_sign_keypair(d + 32, d);
    }

    static void
    enckeygen(llarp::SecretKey &keys)
    {
      auto d = keys.as_array().data();
      randombytes(d, 32);
      crypto_scalarmult_curve25519_base(d + 32, d);
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
    encrypt(PQCipherBlock &ciphertext, SharedSecret &sharedkey,
            const byte_t *pubkey)
    {
      return crypto_kem_enc(ciphertext.as_array().data(),
                            sharedkey.as_array().data(), pubkey)
          != -1;
    }
    bool
    decrypt(const PQCipherBlock &ciphertext, SharedSecret &sharedkey,
            const byte_t *secretkey)
    {
      return crypto_kem_dec(sharedkey.as_array().data(),
                            ciphertext.as_array().data(), secretkey)
          != -1;
    }

    void
    keygen(PQKeyPair &keypair)
    {
      auto d = keypair.as_array().data();
      crypto_kem_keypair(d + PQ_SECRETKEYSIZE, d);
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

  Crypto::Crypto(Crypto::sodium tag)
  {
    (void)tag;
    assert(sodium_init() != -1);
    char *avx2 = std::getenv("AVX2_FORCE_DISABLE");
    if(avx2 && std::string(avx2) == "1")
      ntru_init(1);
    else
      ntru_init(0);
    this->xchacha20           = llarp::sodium::xchacha20;
    this->xchacha20_alt       = llarp::sodium::xchacha20_alt;
    this->dh_client           = llarp::sodium::dh_client;
    this->dh_server           = llarp::sodium::dh_server;
    this->transport_dh_client = llarp::sodium::dh_client;
    this->transport_dh_server = llarp::sodium::dh_server;
    this->hash                = llarp::sodium::hash;
    this->shorthash           = llarp::sodium::shorthash;
    this->hmac                = llarp::sodium::hmac;
    this->sign                = llarp::sodium::sign;
    this->verify              = llarp::sodium::verify;
    this->randomize           = llarp::sodium::randomize;
    this->randbytes           = llarp::sodium::randbytes;
    this->identity_keygen     = llarp::sodium::sigkeygen;
    this->encryption_keygen   = llarp::sodium::enckeygen;
    this->pqe_encrypt         = llarp::pq::encrypt;
    this->pqe_decrypt         = llarp::pq::decrypt;
    this->pqe_keygen          = llarp::pq::keygen;
    int seed                  = 0;
    this->randbytes(&seed, sizeof(seed));
    srand(seed);
  }

  uint64_t
  randint()
  {
    uint64_t i;
    randombytes((byte_t *)&i, sizeof(i));
    return i;
  }

}  // namespace llarp
