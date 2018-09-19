#include <gtest/gtest.h>
#include <llarp/crypto.hpp>
#include <iostream>

namespace llarp
{
  struct PQCryptoTest : public ::testing::Test
  {
    llarp_crypto crypto;
    PQKeyPair keys;

    PQCryptoTest()
    {
      llarp_crypto_init(&crypto);
    }

    llarp_crypto*
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
    ASSERT_TRUE(c->pqe_encrypt(block, shared, pq_keypair_to_public(keys)));
    ASSERT_TRUE(c->pqe_decrypt(block, otherShared, pq_keypair_to_secret(keys)));
    ASSERT_TRUE(otherShared == shared);
  }
}  // namespace llarp
