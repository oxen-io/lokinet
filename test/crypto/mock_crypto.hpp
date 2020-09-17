#ifndef TEST_LLARP_CRYPTO_MOCK_CRYPTO
#define TEST_LLARP_CRYPTO_MOCK_CRYPTO

#include <crypto/crypto.hpp>

#include <gmock/gmock.h>

namespace llarp
{
  namespace test
  {
    struct MockCrypto final : public Crypto
    {
      MOCK_METHOD3(maybe_decrypt_name,std::optional<AlignedBuffer<32>>(std::string_view, llarp::SymmNonce, std::string_view));
    
      
      MOCK_METHOD3(xchacha20,
                   bool(const llarp_buffer_t &, const SharedSecret &,
                        const TunnelNonce &));

      MOCK_METHOD4(xchacha20_alt,
                   bool(const llarp_buffer_t &, const llarp_buffer_t &,
                        const SharedSecret &, const byte_t *));

      MOCK_METHOD4(dh_client,
                   bool(SharedSecret &, const PubKey &, const SecretKey &,
                        const TunnelNonce &));

      MOCK_METHOD4(dh_server,
                   bool(SharedSecret &, const PubKey &, const SecretKey &,
                        const TunnelNonce &));

      MOCK_METHOD4(transport_dh_client,
                   bool(SharedSecret &, const PubKey &, const SecretKey &,
                        const TunnelNonce &));

      MOCK_METHOD4(transport_dh_server,
                   bool(SharedSecret &, const PubKey &, const SecretKey &,
                        const TunnelNonce &));

      MOCK_METHOD2(hash, bool(byte_t *, const llarp_buffer_t &));

      MOCK_METHOD2(shorthash, bool(ShortHash &, const llarp_buffer_t &));

      MOCK_METHOD3(hmac,
                   bool(byte_t *, const llarp_buffer_t &,
                        const SharedSecret &));

      MOCK_METHOD4(derive_subkey, bool(PubKey &, const PubKey &, uint64_t, const AlignedBuffer<32> *));

      MOCK_METHOD4(derive_subkey_private,
                   bool(PrivateKey &, const SecretKey &, uint64_t, const AlignedBuffer<32> *));

      MOCK_METHOD(bool, sign, (Signature &, const SecretKey &, const llarp_buffer_t &));

      MOCK_METHOD(bool, sign, (Signature &, const PrivateKey &, const llarp_buffer_t &));

      MOCK_METHOD3(verify,
                   bool(const PubKey &, const llarp_buffer_t &,
                        const Signature &));

      MOCK_METHOD2(seed_to_secretkey,
                   bool(llarp::SecretKey &, const llarp::IdentitySecret &));

      MOCK_METHOD1(randomize, void(const llarp_buffer_t &));

      MOCK_METHOD2(randbytes, void(byte_t *, size_t));

      MOCK_METHOD1(identity_keygen, void(SecretKey &));

      MOCK_METHOD1(encryption_keygen, void(SecretKey &));

      MOCK_METHOD1(pqe_keygen, void(PQKeyPair &));

      MOCK_METHOD3(pqe_decrypt,
                   bool(const PQCipherBlock &, SharedSecret &, const byte_t *));

      MOCK_METHOD3(pqe_encrypt,
                   bool(PQCipherBlock &, SharedSecret &, const PQPubKey &));

      MOCK_METHOD1(check_identity_privkey, bool(const SecretKey &));
    };
  }  // namespace test
}  // namespace llarp

#endif
