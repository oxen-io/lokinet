#include <crypto/crypto_libsodium.hpp>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_sign.h>
#include <sodium/crypto_scalarmult.h>
#include <sodium/crypto_stream_xchacha20.h>
#include <util/mem.hpp>

#include <cassert>

extern "C"
{
  extern int
  sodium_init(void);
}

#include "ec.hpp"

namespace llarp
{
  namespace sodium
  {
    static bool
    dh(llarp::SharedSecret &out, const PubKey &client_pk,
       const PubKey &server_pk, const uint8_t *themPub, const SecretKey &usSec)
    {
      llarp::SharedSecret shared;
      crypto_generichash_state h;

      if(crypto_scalarmult_curve25519(shared.data(), usSec.data(), themPub))
      {
        return false;
      }
      crypto_generichash_blake2b_init(&h, nullptr, 0U, shared.size());
      crypto_generichash_blake2b_update(&h, client_pk.data(), 32);
      crypto_generichash_blake2b_update(&h, server_pk.data(), 32);
      crypto_generichash_blake2b_update(&h, shared.data(), 32);
      crypto_generichash_blake2b_final(&h, out.data(), shared.size());
      return true;
    }

    static bool
    dh_client_priv(llarp::SharedSecret &shared, const PubKey &pk,
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
    dh_server_priv(llarp::SharedSecret &shared, const PubKey &pk,
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

    CryptoLibSodium::CryptoLibSodium()
    {
      if(sodium_init() == -1)
      {
        throw std::runtime_error("sodium_init() returned -1");
      }
      char *avx2 = std::getenv("AVX2_FORCE_DISABLE");
      if(avx2 && std::string(avx2) == "1")
      {
        ntru_init(1);
      }
      else
      {
        ntru_init(0);
      }
      int seed = 0;
      randombytes(reinterpret_cast< unsigned char * >(&seed), sizeof(seed));
      srand(seed);
    }

    bool
    CryptoLibSodium::xchacha20(const llarp_buffer_t &buff,
                               const SharedSecret &k, const TunnelNonce &n)
    {
      return crypto_stream_xchacha20_xor(buff.base, buff.base, buff.sz,
                                         n.data(), k.data())
          == 0;
    }

    bool
    CryptoLibSodium::xchacha20_alt(const llarp_buffer_t &out,
                                   const llarp_buffer_t &in,
                                   const SharedSecret &k, const byte_t *n)
    {
      if(in.sz > out.sz)
        return false;
      return crypto_stream_xchacha20_xor(out.base, in.base, in.sz, n, k.data())
          == 0;
    }

    bool
    CryptoLibSodium::dh_client(llarp::SharedSecret &shared, const PubKey &pk,
                               const SecretKey &sk, const TunnelNonce &n)
    {
      return dh_client_priv(shared, pk, sk, n);
    }
    /// path dh relay side
    bool
    CryptoLibSodium::dh_server(llarp::SharedSecret &shared, const PubKey &pk,
                               const SecretKey &sk, const TunnelNonce &n)
    {
      return dh_server_priv(shared, pk, sk, n);
    }
    /// transport dh client side
    bool
    CryptoLibSodium::transport_dh_client(llarp::SharedSecret &shared,
                                         const PubKey &pk, const SecretKey &sk,
                                         const TunnelNonce &n)
    {
      return dh_client_priv(shared, pk, sk, n);
    }
    /// transport dh server side
    bool
    CryptoLibSodium::transport_dh_server(llarp::SharedSecret &shared,
                                         const PubKey &pk, const SecretKey &sk,
                                         const TunnelNonce &n)
    {
      return dh_server_priv(shared, pk, sk, n);
    }

    bool
    CryptoLibSodium::shorthash(ShortHash &result, const llarp_buffer_t &buff)
    {
      return crypto_generichash_blake2b(result.data(), ShortHash::SIZE,
                                        buff.base, buff.sz, nullptr, 0)
          != -1;
    }

    bool
    CryptoLibSodium::hmac(byte_t *result, const llarp_buffer_t &buff,
                          const SharedSecret &secret)
    {
      return crypto_generichash_blake2b(result, HMACSIZE, buff.base, buff.sz,
                                        secret.data(), HMACSECSIZE)
          != -1;
    }

    using ec_scalar = AlignedBuffer< 32 >;

    struct s_comm final
        : public AlignedBuffer< LongHash::SIZE + (ec_scalar::SIZE * 2) >
    {
      byte_t *
      H()
      {
        return data();
      }

      byte_t *
      K()
      {
        return data() + LongHash::SIZE;
      }

      byte_t *
      C()
      {
        return data() + LongHash::SIZE + ec_scalar::SIZE;
      }
    };

    static inline bool
    IsNonZero(const byte_t *s)
    {
      return (((int)(s[0] | s[1] | s[2] | s[3] | s[4] | s[5] | s[6] | s[7]
                     | s[8] | s[9] | s[10] | s[11] | s[12] | s[13] | s[14]
                     | s[15] | s[16] | s[17] | s[18] | s[19] | s[20] | s[21]
                     | s[22] | s[23] | s[24] | s[25] | s[26] | s[27] | s[28]
                     | s[29] | s[30] | s[31])
               - 1)
              >> 8)
          + 1;
    }

    static inline bool
    less32(const unsigned char *k0, const unsigned char *k1)
    {
      for(int n = 31; n >= 0; --n)
      {
        if(k0[n] < k1[n])
          return true;
        if(k0[n] > k1[n])
          return false;
      }
      return false;
    }

    static void
    rand32_unbais(byte_t *k)
    {
      // l = 2^252 + 27742317777372353535851937790883648493.
      // it fits 15 in 32 bytes
      static const unsigned char limit[32] = {
          0xe3, 0x6a, 0x67, 0x72, 0x8b, 0xce, 0x13, 0x29, 0x8f, 0x30, 0x82,
          0x8c, 0x0b, 0xa4, 0x10, 0x39, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0};
      do
      {
        randombytes(k, 32);
      } while(!IsNonZero(k) && !less32(k, limit));
      sc25519_reduce32(k);
    }

    static bool
    check_key(const byte_t *k)
    {
      ge25519_p3 p;
      return ge25519_frombytes_vartime(&p, k) == 0;
    }

    template < typename T, size_t outsz = ec_scalar::SIZE >
    static inline bool
    hash_to_scalar(const T &in, byte_t *out)
    {
      if(crypto_generichash_blake2b(out, outsz, in.data(), in.size(), nullptr,
                                    0)
         == -1)
        return false;
      sc25519_reduce32(out);
      return true;
    }

    static bool
    hash(uint8_t *result, const llarp_buffer_t &buff)
    {
      return crypto_generichash_blake2b(result, HASHSIZE, buff.base, buff.sz,
                                        nullptr, 0)
          != -1;
    }

    bool
    CryptoLibSodium::sign(Signature &sig, const SecretKey &secret,
                          const llarp_buffer_t &buf)
    {
      s_comm tmp;
      if(!hash(tmp.H(), buf))
        return false;
      ge25519_p3 tmp3;
      ec_scalar K;
      const PubKey pk = secret.toPublic();
      std::copy_n(pk.begin(), pk.size(), tmp.K());
      do
      {
        rand32_unbais(K.data());
        if(((const uint32_t *)(K.data()))[7]
           == 0)  // we don't want tiny numbers here
          continue;
        ge25519_scalarmult_base(&tmp3, K.data());
        ge25519_p3_tobytes(tmp.C(), &tmp3);
        hash_to_scalar(tmp, sig.C());
        if(!IsNonZero(sig.C()))
          continue;
        sc25519_mulsub(sig.R(), sig.C(), secret.data(), K.data());
        if(!IsNonZero(sig.R()))
          continue;
        return true;
      } while(true);
    }

    bool
    CryptoLibSodium::verify(const PubKey &pub, const llarp_buffer_t &buf,
                            const Signature &sig)
    {
      s_comm tmp;
      ge25519_p2 tmp2;
      ge25519_p3 tmp3;
      ec_scalar C;
      std::copy_n(pub.begin(), pub.size(), tmp.K());
      if(!hash(tmp.H(), buf))
        return false;
      if(ge25519_frombytes_vartime(&tmp3, pub.data()) != 0)
        return false;
      if(sc25519_check(sig.C()) != 0 || sc25519_check(sig.R()) != 0
         || !IsNonZero(sig.C()))
        return false;
      ge25519_double_scalarmult_base_vartime(&tmp2, sig.C(), &tmp3, sig.R());
      ge25519_tobytes(tmp.C(), &tmp2);
      static const ec_scalar infinity{{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
      if(memcmp(tmp.C(), &infinity, 32) == 0)
        return false;
      hash_to_scalar(tmp, C.data());
      sc25519_sub(C.data(), C.data(), sig.C());
      return !IsNonZero(C.data());
    }

    bool
    CryptoLibSodium::seed_to_secretkey(llarp::SecretKey &secret,
                                       const llarp::IdentitySecret &seed)
    {
      if(sc25519_check(seed.data()) != 0)
        return false;
      ge25519_p3 A;
      std::copy_n(seed.begin(), 32, secret.begin());
      ge25519_scalarmult_base(&A, seed.data());
      byte_t *pub = secret.data() + 32;
      ge25519_p3_tobytes(pub, &A);
      return true;
    }

    void
    CryptoLibSodium::randomize(const llarp_buffer_t &buff)
    {
      randombytes((unsigned char *)buff.base, buff.sz);
    }

    void
    CryptoLibSodium::randbytes(byte_t *ptr, size_t sz)
    {
      randombytes((unsigned char *)ptr, sz);
    }

    void
    CryptoLibSodium::identity_keygen(llarp::SecretKey &keys)
    {
      llarp::IdentitySecret seed;
      rand32_unbais(seed.data());
      seed_to_secretkey(keys, seed);
    }

    void
    CryptoLibSodium::encryption_keygen(llarp::SecretKey &keys)
    {
      auto d = keys.data();
      rand32_unbais(d);
      crypto_scalarmult_curve25519_base(d + 32, d);
    }

    bool
    CryptoLibSodium::pqe_encrypt(PQCipherBlock &ciphertext,
                                 SharedSecret &sharedkey,
                                 const PQPubKey &pubkey)
    {
      return crypto_kem_enc(ciphertext.data(), sharedkey.data(), pubkey.data())
          != -1;
    }
    bool
    CryptoLibSodium::pqe_decrypt(const PQCipherBlock &ciphertext,
                                 SharedSecret &sharedkey,
                                 const byte_t *secretkey)
    {
      return crypto_kem_dec(sharedkey.data(), ciphertext.data(), secretkey)
          != -1;
    }

    void
    CryptoLibSodium::pqe_keygen(PQKeyPair &keypair)
    {
      auto d = keypair.data();
      crypto_kem_keypair(d + PQ_SECRETKEYSIZE, d);
    }
  }  // namespace sodium

  const byte_t *
  seckey_topublic(const SecretKey &sec)
  {
    return sec.data() + 32;
  }

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

  uint64_t
  randint()
  {
    uint64_t i;
    randombytes((byte_t *)&i, sizeof(i));
    return i;
  }

}  // namespace llarp
