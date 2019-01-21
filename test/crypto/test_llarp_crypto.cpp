#include <crypto/crypto.hpp>

#include <iostream>

#include <gtest/gtest.h>

namespace llarp
{
  struct IdentityKeyTest : public ::testing::Test 
  {
    llarp::Crypto crypto;
    llarp::IdentitySecret seed;

    IdentityKeyTest() : crypto(llarp::Crypto::sodium{})
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
      seed.Randomize();
    }


  };

  TEST_F(IdentityKeyTest, TestSeedToSecretKey)
  {
    SecretKey secret;
    ASSERT_TRUE(crypto.seed_to_secretkey(secret, seed));
    AlignedBuffer<128> random;
    random.Randomize();
    Signature sig;
    ASSERT_TRUE(crypto.sign(sig, secret, random.as_buffer()));
    ASSERT_TRUE(crypto.verify(secret.toPublic(), random.as_buffer(), sig));
    // mangle sig
    sig.Randomize();
    ASSERT_FALSE(crypto.verify(secret.toPublic(), random.as_buffer(), sig));
  }

  struct PQCryptoTest : public ::testing::Test
  {
    llarp::Crypto crypto;
    PQKeyPair keys;

    PQCryptoTest() : crypto(llarp::Crypto::sodium{})
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
