#ifndef LLARP_CRYPTO_LIBSODIUM_HPP
#define LLARP_CRYPTO_LIBSODIUM_HPP

#include <crypto/crypto.hpp>

namespace llarp
{
  namespace sodium
  {
    struct CryptoLibSodium final : public Crypto
    {
      CryptoLibSodium();

      ~CryptoLibSodium()
      {
      }

      /// xchacha symmetric cipher
      bool
      xchacha20(llarp_buffer_t, const SharedSecret &,
                const TunnelNonce &) override;

      /// xchacha symmetric cipher (multibuffer)
      bool
      xchacha20_alt(llarp_buffer_t, llarp_buffer_t, const SharedSecret &,
                    const byte_t *) override;

      /// path dh creator's side
      bool
      dh_client(SharedSecret &, const PubKey &, const SecretKey &,
                const TunnelNonce &) override;
      /// path dh relay side
      bool
      dh_server(SharedSecret &, const PubKey &, const SecretKey &,
                const TunnelNonce &) override;
      /// transport dh client side
      bool
      transport_dh_client(SharedSecret &, const PubKey &, const SecretKey &,
                          const TunnelNonce &) override;
      /// transport dh server side
      bool
      transport_dh_server(SharedSecret &, const PubKey &, const SecretKey &,
                          const TunnelNonce &) override;
      /// blake2b 512 bit
      bool
      hash(byte_t *, llarp_buffer_t) override;
      /// blake2b 256 bit
      bool
      shorthash(ShortHash &, llarp_buffer_t) override;
      /// blake2s 256 bit hmac
      bool
      hmac(byte_t *, llarp_buffer_t, const SharedSecret &) override;
      /// ed25519 sign
      bool
      sign(Signature &, const SecretKey &, llarp_buffer_t) override;
      /// ed25519 verify
      bool
      verify(const PubKey &, llarp_buffer_t, const Signature &) override;
      /// seed to secretkey
      bool
      seed_to_secretkey(llarp::SecretKey &,
                        const llarp::IdentitySecret &) override;
      /// randomize buffer
      void randomize(llarp_buffer_t) override;
      /// randomizer memory
      void
      randbytes(void *, size_t) override;
      /// generate signing keypair
      void
      identity_keygen(SecretKey &) override;
      /// generate encryption keypair
      void
      encryption_keygen(SecretKey &) override;
      /// generate post quantum encrytion key
      void
      pqe_keygen(PQKeyPair &) override;
      /// post quantum decrypt (buffer, sharedkey_dst, sec)
      bool
      pqe_decrypt(const PQCipherBlock &, SharedSecret &,
                  const byte_t *) override;
      /// post quantum encrypt (buffer, sharedkey_dst,  pub)
      bool
      pqe_encrypt(PQCipherBlock &, SharedSecret &, const PQPubKey &) override;
    };
  }  // namespace sodium

}  // namespace llarp

#endif
