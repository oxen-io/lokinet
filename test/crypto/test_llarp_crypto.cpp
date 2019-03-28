#include <crypto/crypto_libsodium.hpp>

#include <iostream>

#include <gtest/gtest.h>

namespace llarp
{
  struct IdentityKeyTest : public ::testing::Test
  {
    llarp::sodium::CryptoLibSodium crypto;

    IdentityKeyTest()
    {
    }

    llarp::Crypto*
    Crypto()
    {
      return &crypto;
    }
  };

  TEST_F(IdentityKeyTest, TestSignVerify)
  {
    SecretKey secret;
    crypto.identity_keygen(secret);
    AlignedBuffer< 128 > random;
    random.Randomize();
    Signature sig;

    llarp_buffer_t buf(random);
    ASSERT_TRUE(crypto.sign(sig, secret, buf));
    ASSERT_TRUE(crypto.verify(secret.toPublic(), buf, sig));
    random.Randomize();
    // mangle body
    ASSERT_FALSE(crypto.verify(secret.toPublic(), buf, sig));
  }

  struct PQCryptoTest : public ::testing::Test
  {
    llarp::sodium::CryptoLibSodium crypto;
    PQKeyPair keys;

    PQCryptoTest()
    {
    }

    llarp::Crypto*
    Crypto()
    {
      return &crypto;
    }

    void
    SetUp()
    {
      crypto.pqe_keygen(keys);
    }
  };

  TEST_F(PQCryptoTest, TestCrypto)
  {
    PQCipherBlock block;
    SharedSecret shared, otherShared;
    auto c = Crypto();

    ASSERT_TRUE(keys.size() == PQ_KEYPAIRSIZE);
    ASSERT_TRUE(
        c->pqe_encrypt(block, shared, PQPubKey(pq_keypair_to_public(keys))));
    ASSERT_TRUE(c->pqe_decrypt(block, otherShared, pq_keypair_to_secret(keys)));
    ASSERT_TRUE(otherShared == shared);
  }
}  // namespace llarp
