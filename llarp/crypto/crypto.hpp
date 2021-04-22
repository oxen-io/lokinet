#pragma once

#include "constants.hpp"
#include "types.hpp"

#include <llarp/util/buffer.hpp>

#include <functional>

#include <cstdint>

/**
 * crypto.hpp
 *
 * libsodium abstraction layer
 * potentially allow libssl support in the future
 */

namespace llarp
{
  /// library crypto configuration
  struct Crypto
  {
    virtual ~Crypto() = 0;

    /// decrypt cipherText name given the key generated from name
    virtual std::optional<AlignedBuffer<32>>
    maybe_decrypt_name(std::string_view ciphertext, SymmNonce nounce, std::string_view name) = 0;

    /// xchacha symmetric cipher
    virtual bool
    xchacha20(const llarp_buffer_t&, const SharedSecret&, const TunnelNonce&) = 0;

    /// xchacha symmetric cipher (multibuffer)
    virtual bool
    xchacha20_alt(
        const llarp_buffer_t&, const llarp_buffer_t&, const SharedSecret&, const byte_t*) = 0;

    /// path dh creator's side
    virtual bool
    dh_client(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) = 0;
    /// path dh relay side
    virtual bool
    dh_server(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) = 0;
    /// transport dh client side
    virtual bool
    transport_dh_client(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) = 0;
    /// transport dh server side
    virtual bool
    transport_dh_server(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) = 0;
    /// blake2b 256 bit
    virtual bool
    shorthash(ShortHash&, const llarp_buffer_t&) = 0;
    /// blake2s 256 bit "hmac" (keyed hash)
    virtual bool
    hmac(byte_t*, const llarp_buffer_t&, const SharedSecret&) = 0;
    /// ed25519 sign
    virtual bool
    sign(Signature&, const SecretKey&, const llarp_buffer_t&) = 0;
    /// ed25519 sign (custom with derived keys)
    virtual bool
    sign(Signature&, const PrivateKey&, const llarp_buffer_t&) = 0;
    /// ed25519 verify
    virtual bool
    verify(const PubKey&, const llarp_buffer_t&, const Signature&) = 0;

    /// derive sub keys for public keys
    virtual bool
    derive_subkey(PubKey&, const PubKey&, uint64_t, const AlignedBuffer<32>* = nullptr) = 0;

    /// derive sub keys for private keys
    virtual bool
    derive_subkey_private(
        PrivateKey&, const SecretKey&, uint64_t, const AlignedBuffer<32>* = nullptr) = 0;

    /// seed to secretkey
    virtual bool
    seed_to_secretkey(llarp::SecretKey&, const llarp::IdentitySecret&) = 0;
    /// randomize buffer
    virtual void
    randomize(const llarp_buffer_t&) = 0;
    /// randomizer memory
    virtual void
    randbytes(byte_t*, size_t) = 0;
    /// generate signing keypair
    virtual void
    identity_keygen(SecretKey&) = 0;
    /// generate encryption keypair
    virtual void
    encryption_keygen(SecretKey&) = 0;
    /// generate post quantum encrytion key
    virtual void
    pqe_keygen(PQKeyPair&) = 0;
    /// post quantum decrypt (buffer, sharedkey_dst, sec)
    virtual bool
    pqe_decrypt(const PQCipherBlock&, SharedSecret&, const byte_t*) = 0;
    /// post quantum encrypt (buffer, sharedkey_dst,  pub)
    virtual bool
    pqe_encrypt(PQCipherBlock&, SharedSecret&, const PQPubKey&) = 0;

    virtual bool
    check_identity_privkey(const SecretKey&) = 0;
  };

  inline Crypto::~Crypto() = default;

  /// return random 64bit unsigned interger
  uint64_t
  randint();

  const byte_t*
  seckey_topublic(const SecretKey& secret);

  const byte_t*
  pq_keypair_to_public(const PQKeyPair& keypair);

  const byte_t*
  pq_keypair_to_secret(const PQKeyPair& keypair);

  struct CryptoManager
  {
   private:
    static Crypto* m_crypto;

    Crypto* m_prevCrypto;

   public:
    explicit CryptoManager(Crypto* crypto) : m_prevCrypto(m_crypto)
    {
      m_crypto = crypto;
    }

    ~CryptoManager()
    {
      m_crypto = m_prevCrypto;
    }

    static Crypto*
    instance()
    {
#ifdef NDEBUG
      return m_crypto;
#else
      if (m_crypto)
        return m_crypto;

      assert(false && "Cryptomanager::instance() was undefined");
      abort();
#endif
    }
  };

  /// rng type that uses llarp::randint(), which is cryptographically secure
  struct CSRNG
  {
    using result_type = uint64_t;

    static constexpr uint64_t
    min()
    {
      return std::numeric_limits<uint64_t>::min();
    };

    static constexpr uint64_t
    max()
    {
      return std::numeric_limits<uint64_t>::max();
    };

    uint64_t
    operator()()
    {
      return llarp::randint();
    };
  };

}  // namespace llarp
