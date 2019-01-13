#ifndef LLARP_CRYPTO_HPP
#define LLARP_CRYPTO_HPP

#include <router_id.hpp>
#include <util/aligned.hpp>
#include <util/buffer.h>
#include <util/common.hpp>
#include <util/mem.h>
#include <util/threadpool.h>

#include <functional>
#include <stdbool.h>
#include <stdint.h>

/**
 * crypto.hpp
 *
 * libsodium abstraction layer
 * potentially allow libssl support in the future
 */

static constexpr uint32_t PUBKEYSIZE    = 32;
static constexpr uint32_t SECKEYSIZE    = 64;
static constexpr uint32_t NONCESIZE     = 24;
static constexpr uint32_t SHAREDKEYSIZE = 32;
static constexpr uint32_t HASHSIZE      = 64;
static constexpr uint32_t SHORTHASHSIZE = 32;
static constexpr uint32_t HMACSECSIZE   = 32;
static constexpr uint32_t SIGSIZE       = 64;
static constexpr uint32_t TUNNONCESIZE  = 32;
static constexpr uint32_t HMACSIZE      = 32;
static constexpr uint32_t PATHIDSIZE    = 16;

#include <libntrup/ntru.h>

#define PQ_CIPHERTEXTSIZE crypto_kem_CIPHERTEXTBYTES
#define PQ_PUBKEYSIZE crypto_kem_PUBLICKEYBYTES
#define PQ_SECRETKEYSIZE crypto_kem_SECRETKEYBYTES
#define PQ_KEYPAIRSIZE (PQ_SECRETKEYSIZE + PQ_PUBKEYSIZE)

namespace llarp
{
  using SharedSecret     = AlignedBuffer< SHAREDKEYSIZE >;
  using KeyExchangeNonce = AlignedBuffer< 32 >;

  struct PubKey final : public AlignedBuffer< PUBKEYSIZE >
  {
    PubKey() : AlignedBuffer< SIZE >()
    {
    }

    explicit PubKey(const byte_t *ptr) : AlignedBuffer< SIZE >(ptr)
    {
    }

    explicit PubKey(const Data &data) : AlignedBuffer< SIZE >(data)
    {
    }

    explicit PubKey(const AlignedBuffer< SIZE > &other)
        : AlignedBuffer< SIZE >(other)
    {
    }

    std::string
    ToString() const;

    bool
    FromString(const std::string &str);

    friend std::ostream &
    operator<<(std::ostream &out, const PubKey &k)
    {
      return out << k.ToString();
    }

    operator RouterID() const
    {
      return RouterID(as_array());
    }

    PubKey &
    operator=(const byte_t *ptr)
    {
      std::copy(ptr, ptr + SIZE, begin());
      return *this;
    }
  };

  struct SecretKey final : public AlignedBuffer< SECKEYSIZE >
  {
    SecretKey() : AlignedBuffer< SECKEYSIZE >(){};

    explicit SecretKey(const SecretKey &k) : AlignedBuffer< SECKEYSIZE >(k)
    {
    }

    explicit SecretKey(const byte_t *ptr) : AlignedBuffer< SECKEYSIZE >(ptr)
    {
    }

    friend std::ostream &
    operator<<(std::ostream &out, const SecretKey &)
    {
      // make sure we never print out secret keys
      return out << "[secretkey]";
    }

    PubKey
    toPublic() const
    {
      return PubKey(data() + 32);
    }

    bool
    LoadFromFile(const char *fname);

    bool
    SaveToFile(const char *fname) const;

    SecretKey &
    operator=(const byte_t *ptr)
    {
      std::copy(ptr, ptr + SIZE, begin());
      return *this;
    }
  };

  using ShortHash   = AlignedBuffer< SHORTHASHSIZE >;
  using Signature   = AlignedBuffer< SIGSIZE >;
  using TunnelNonce = AlignedBuffer< TUNNONCESIZE >;
  using SymmNonce   = AlignedBuffer< NONCESIZE >;
  using SymmKey     = AlignedBuffer< 32 >;

  using PQCipherBlock = AlignedBuffer< PQ_CIPHERTEXTSIZE + 1 >;
  using PQPubKey      = AlignedBuffer< PQ_PUBKEYSIZE >;
  using PQKeyPair     = AlignedBuffer< PQ_KEYPAIRSIZE >;

  /// label functors

  /// PKE(result, publickey, secretkey, nonce)
  using path_dh_func = std::function< bool(
      SharedSecret &, const PubKey &, const SecretKey &, const TunnelNonce &) >;

  /// TKE(result, publickey, secretkey, nonce)
  using transport_dh_func = std::function< bool(
      SharedSecret &, const PubKey &, const SecretKey &, const TunnelNonce &) >;

  /// SD/SE(buffer, key, nonce)
  using sym_cipher_func = std::function< bool(
      llarp_buffer_t, const SharedSecret &, const TunnelNonce &) >;

  /// SD/SE(dst, src, key, nonce)
  using sym_cipher_alt_func = std::function< bool(
      llarp_buffer_t, llarp_buffer_t, const SharedSecret &, const byte_t *) >;

  /// H(result, body)
  using hash_func = std::function< bool(byte_t *, llarp_buffer_t) >;

  /// SH(result, body)
  using shorthash_func = std::function< bool(ShortHash &, llarp_buffer_t) >;

  /// MDS(result, body, shared_secret)
  using hmac_func =
      std::function< bool(byte_t *, llarp_buffer_t, const SharedSecret &) >;

  /// S(sig, secretkey, body)
  using sign_func =
      std::function< bool(Signature &, const SecretKey &, llarp_buffer_t) >;

  /// V(pubkey, body, sig)
  using verify_func =
      std::function< bool(const PubKey &, llarp_buffer_t, const Signature &) >;

  /// library crypto configuration
  struct Crypto
  {
    /// xchacha symmetric cipher
    sym_cipher_func xchacha20;
    /// xchacha symmetric cipher (multibuffer)
    sym_cipher_alt_func xchacha20_alt;
    /// path dh creator's side
    path_dh_func dh_client;
    /// path dh relay side
    path_dh_func dh_server;
    /// transport dh client side
    transport_dh_func transport_dh_client;
    /// transport dh server side
    transport_dh_func transport_dh_server;
    /// blake2b 512 bit
    hash_func hash;
    /// blake2b 256 bit
    shorthash_func shorthash;
    /// blake2s 256 bit hmac
    hmac_func hmac;
    /// ed25519 sign
    sign_func sign;
    /// ed25519 verify
    verify_func verify;
    /// randomize buffer
    std::function< void(llarp_buffer_t) > randomize;
    /// randomizer memory
    std::function< void(void *, size_t) > randbytes;
    /// generate signing keypair
    std::function< void(SecretKey &) > identity_keygen;
    /// generate encryption keypair
    std::function< void(SecretKey &) > encryption_keygen;
    /// generate post quantum encrytion key
    std::function< void(PQKeyPair &) > pqe_keygen;
    /// post quantum decrypt (buffer, sharedkey_dst, sec)
    std::function< bool(const PQCipherBlock &, SharedSecret &, const byte_t *) >
        pqe_decrypt;
    /// post quantum encrypt (buffer, sharedkey_dst,  pub)
    std::function< bool(PQCipherBlock &, SharedSecret &, const PQPubKey &) >
        pqe_encrypt;

    // Give a basic type tag for the constructor to pick libsodium
    struct sodium
    {
    };

    Crypto(Crypto::sodium tag);
  };

  /// return random 64bit unsigned interger
  uint64_t
  randint();

  const byte_t *
  seckey_topublic(const SecretKey &secret);

  const byte_t *
  pq_keypair_to_public(const PQKeyPair &keypair);

  const byte_t *
  pq_keypair_to_secret(const PQKeyPair &keypair);

}  // namespace llarp

#endif
