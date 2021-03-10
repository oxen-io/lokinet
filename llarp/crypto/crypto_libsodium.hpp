#pragma once

#include "crypto.hpp"

namespace llarp
{
  namespace sodium
  {
    struct CryptoLibSodium final : public Crypto
    {
      CryptoLibSodium();

      ~CryptoLibSodium() override = default;

      /// decrypt cipherText given the key generated from name
      std::optional<AlignedBuffer<32>>
      maybe_decrypt_name(
          std::string_view ciphertext, SymmNonce nounce, std::string_view name) override;

      /// xchacha symmetric cipher
      bool
      xchacha20(const llarp_buffer_t&, const SharedSecret&, const TunnelNonce&) override;

      /// xchacha symmetric cipher (multibuffer)
      bool
      xchacha20_alt(
          const llarp_buffer_t&,
          const llarp_buffer_t&,
          const SharedSecret&,
          const byte_t*) override;

      /// path dh creator's side
      bool
      dh_client(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) override;
      /// path dh relay side
      bool
      dh_server(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) override;
      /// transport dh client side
      bool
      transport_dh_client(
          SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) override;
      /// transport dh server side
      bool
      transport_dh_server(
          SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&) override;
      /// blake2b 256 bit
      bool
      shorthash(ShortHash&, const llarp_buffer_t&) override;
      /// blake2s 256 bit hmac
      bool
      hmac(byte_t*, const llarp_buffer_t&, const SharedSecret&) override;
      /// ed25519 sign
      bool
      sign(Signature&, const SecretKey&, const llarp_buffer_t&) override;
      /// ed25519 sign (custom with derived keys)
      bool
      sign(Signature&, const PrivateKey&, const llarp_buffer_t&) override;
      /// ed25519 verify
      bool
      verify(const PubKey&, const llarp_buffer_t&, const Signature&) override;

      /// derive sub keys for public keys.  hash is really only intended for
      /// testing and overrides key_n if given.
      bool
      derive_subkey(
          PubKey& derived,
          const PubKey& root,
          uint64_t key_n,
          const AlignedBuffer<32>* hash = nullptr) override;

      /// derive sub keys for private keys.  hash is really only intended for
      /// testing and overrides key_n if given.
      bool
      derive_subkey_private(
          PrivateKey& derived,
          const SecretKey& root,
          uint64_t key_n,
          const AlignedBuffer<32>* hash = nullptr) override;

      /// seed to secretkey
      bool
      seed_to_secretkey(llarp::SecretKey&, const llarp::IdentitySecret&) override;
      /// randomize buffer
      void
      randomize(const llarp_buffer_t&) override;
      /// randomizer memory
      void
      randbytes(byte_t*, size_t) override;
      /// generate signing keypair
      void
      identity_keygen(SecretKey&) override;
      /// generate encryption keypair
      void
      encryption_keygen(SecretKey&) override;
      /// generate post quantum encrytion key
      void
      pqe_keygen(PQKeyPair&) override;
      /// post quantum decrypt (buffer, sharedkey_dst, sec)
      bool
      pqe_decrypt(const PQCipherBlock&, SharedSecret&, const byte_t*) override;
      /// post quantum encrypt (buffer, sharedkey_dst,  pub)
      bool
      pqe_encrypt(PQCipherBlock&, SharedSecret&, const PQPubKey&) override;

      bool
      check_identity_privkey(const SecretKey&) override;
    };
  }  // namespace sodium

}  // namespace llarp
