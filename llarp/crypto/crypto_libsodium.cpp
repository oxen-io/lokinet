#include <crypto/crypto.hpp>

#include <sodium/crypto_generichash.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <util/mem.hpp>

#include <assert.h>

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
    xchacha20(llarp_buffer_t buff, const SharedSecret &k, const TunnelNonce &n)
    {
      return crypto_stream_xchacha20_xor(buff.base, buff.base, buff.sz,
                                         n.data(), k.data())
          == 0;
    }

    static bool
    xchacha20_alt(llarp_buffer_t out, llarp_buffer_t in, const SharedSecret &k,
                  const byte_t *n)
    {
      if(in.sz > out.sz)
        return false;
      return crypto_stream_xchacha20_xor(out.base, in.base, in.sz, n, k.data())
          == 0;
    }

    static bool
    dh(llarp::SharedSecret &out, const PubKey &client_pk,
       const PubKey &server_pk, const uint8_t *themPub, const SecretKey &usSec)
    {
      llarp::SharedSecret shared;
      crypto_generichash_state h;

      if(crypto_scalarmult_curve25519(shared.data(), usSec.data(), themPub))
        return false;
      crypto_generichash_blake2b_init(&h, nullptr, 0U, shared.size());
      crypto_generichash_blake2b_update(&h, client_pk.data(), 32);
      crypto_generichash_blake2b_update(&h, server_pk.data(), 32);
      crypto_generichash_blake2b_update(&h, shared.data(), 32);
      crypto_generichash_blake2b_final(&h, out.data(), shared.size());
      return true;
    }

    static bool
    dh_client(llarp::SharedSecret &shared, const PubKey &pk,
              const SecretKey &sk, const TunnelNonce &n)
    {
      llarp::SharedSecret dh_result;

      if(dh(dh_result, sk.toPublic(), pk, pk.data(), sk))
      {
        return crypto_generichash_blake2b(shared.data(), 32, n.data(), 32,
                                          dh_result.data(), 32)
            != -1;
      }
      llarp::LogWarn("crypto::dh_client - dh failed");
      return false;
    }

    static bool
    dh_server(llarp::SharedSecret &shared, const PubKey &pk,
              const SecretKey &sk, const TunnelNonce &n)
    {
      llarp::SharedSecret dh_result;
      if(dh(dh_result, pk, sk.toPublic(), pk.data(), sk))
      {
        return crypto_generichash_blake2b(shared.data(), 32, n.data(), 32,
                                          dh_result.data(), 32)
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
      return crypto_generichash_blake2b(result.data(), ShortHash::SIZE,
                                        buff.base, buff.sz, nullptr, 0)
          != -1;
    }

    static bool
    hmac(byte_t *result, llarp_buffer_t buff, const SharedSecret &secret)
    {
      return crypto_generichash_blake2b(result, HMACSIZE, buff.base, buff.sz,
                                        secret.data(), HMACSECSIZE)
          != -1;
    }

    static bool
    sign(Signature &result, const SecretKey &secret, llarp_buffer_t buff)
    {
      int rc = crypto_sign_detached(result.data(), nullptr, buff.base, buff.sz,
                                    secret.data());
      return rc != -1;
    }

    static bool
    verify(const PubKey &pub, llarp_buffer_t buff, const Signature &sig)
    {
      int rc = crypto_sign_verify_detached(sig.data(), buff.base, buff.sz,
                                           pub.data());
      return rc != -1;
    }

    static bool
    seed_to_secretkey(llarp::SecretKey &secret,
                      const llarp::IdentitySecret &seed)
    {
      byte_t pk[crypto_sign_ed25519_PUBLICKEYBYTES];
      return crypto_sign_ed25519_seed_keypair(pk, secret.data(), seed.data())
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
      byte_t *d = keys.data();
      crypto_sign_keypair(d + 32, d);
    }

    static void
    enckeygen(llarp::SecretKey &keys)
    {
      auto d = keys.data();
      randombytes(d, 32);
      crypto_scalarmult_curve25519_base(d + 32, d);
    }
  }  // namespace sodium

  const byte_t *
  seckey_topublic(const SecretKey &sec)
  {
    return sec.data() + 32;
  }

  namespace pq
  {
    bool
    encrypt(PQCipherBlock &ciphertext, SharedSecret &sharedkey,
            const PQPubKey &pubkey)
    {
      return crypto_kem_enc(ciphertext.data(), sharedkey.data(), pubkey.data())
          != -1;
    }
    bool
    decrypt(const PQCipherBlock &ciphertext, SharedSecret &sharedkey,
            const byte_t *secretkey)
    {
      return crypto_kem_dec(sharedkey.data(), ciphertext.data(), secretkey)
          != -1;
    }

    void
    keygen(PQKeyPair &keypair)
    {
      auto d = keypair.data();
      crypto_kem_keypair(d + PQ_SECRETKEYSIZE, d);
    }
  }  // namespace pq

  const byte_t *
  pq_keypair_to_public(const PQKeyPair &k)
  {
    return k.data() + PQ_SECRETKEYSIZE;
  }

  const byte_t *
  pq_keypair_to_secret(const PQKeyPair &k)
  {
    return k.data();
  }

  Crypto::Crypto(Crypto::sodium tag)
  {
    (void)tag;
    if(sodium_init() == -1)
      throw std::runtime_error("sodium_init() returned -1");
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
    this->seed_to_secretkey   = llarp::sodium::seed_to_secretkey;
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
