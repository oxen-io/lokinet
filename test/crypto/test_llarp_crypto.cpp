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
  };

  TEST_F(IdentityKeyTest, TestKeyGen)
  {
    SecretKey secret;
    crypto.identity_keygen(secret);
    ASSERT_FALSE(secret.IsZero());
  }

  TEST_F(IdentityKeyTest, TestSignVerify)
  {
    SecretKey secret;
    crypto.identity_keygen(secret);
    AlignedBuffer< 128 > random;
    random.Randomize();
    Signature sig;
    const PubKey pk = secret.toPublic();

    const llarp_buffer_t buf(random.data(), random.size());
    ASSERT_TRUE(crypto.sign(sig, secret, buf));
    ASSERT_TRUE(crypto.verify(pk, buf, sig));
    random.Randomize();
    // mangle body
    ASSERT_FALSE(crypto.verify(pk, buf, sig));
  }

  struct PQCryptoTest : public ::testing::Test
  {
    llarp::sodium::CryptoLibSodium crypto;
    PQKeyPair keys;

    PQCryptoTest()
    {
    }

    void
    SetUp()
    {
      crypto.pqe_keygen(keys);
    }
  };

  TEST_F(PQCryptoTest, TestCrypto)
  {
    ASSERT_FALSE(keys.IsZero());
    PQCipherBlock block;
    SharedSecret shared, otherShared;
    ASSERT_TRUE(shared.IsZero());
    ASSERT_TRUE(otherShared.IsZero());
    ASSERT_TRUE(block.IsZero());
    auto c = &crypto;

    ASSERT_TRUE(keys.size() == PQ_KEYPAIRSIZE);
    ASSERT_TRUE(
        c->pqe_encrypt(block, shared, PQPubKey(pq_keypair_to_public(keys))));
    ASSERT_TRUE(c->pqe_decrypt(block, otherShared, pq_keypair_to_secret(keys)));
    ASSERT_FALSE(shared.IsZero());
    ASSERT_FALSE(otherShared.IsZero());
    ASSERT_TRUE(otherShared == shared);
    ASSERT_FALSE(block.IsZero());
  }

  TEST_F(PQCryptoTest, TestAVX2)
  {
#if __AVX2__
    llarp::AlignedBuffer<crypto_kem_PUBLICKEYBYTES> pk0;
    llarp::AlignedBuffer<crypto_kem_SECRETKEYBYTES> sk0;
    llarp::AlignedBuffer<crypto_kem_CIPHERTEXTBYTES> ct0;
    llarp::AlignedBuffer<32> k0, k1, k2, k3;
    ASSERT_TRUE(k0.IsZero());
    ASSERT_TRUE(k1.IsZero());
    ASSERT_TRUE(ct0.IsZero());
    ASSERT_TRUE(pk0.IsZero());
    ASSERT_TRUE(sk0.IsZero());
   
    ASSERT_FALSE(crypto_kem_keypair_avx2(pk0.data(), sk0.data()) == -1);
    ASSERT_FALSE(crypto_kem_enc_avx2(ct0.data(), k0.data(), pk0.data()) == -1);
    ASSERT_FALSE(ct0.IsZero());
    ASSERT_FALSE(pk0.IsZero());
    ASSERT_FALSE(sk0.IsZero());
    ASSERT_FALSE(crypto_kem_dec_avx2(k0.data(), ct0.data(), sk0.data()) == -1);
    ASSERT_FALSE(crypto_kem_dec_ref(k1.data(), ct0.data(), sk0.data()) == -1);
    ASSERT_FALSE(k0.IsZero());
    ASSERT_FALSE(k1.IsZero());
    ASSERT_EQ(k0, k1);
    ASSERT_TRUE(k2.IsZero());
    ASSERT_TRUE(k3.IsZero());
    ASSERT_FALSE(crypto_kem_dec_avx2(k2.data(), ct0.data(), sk0.data()) == -1);
    ASSERT_FALSE(crypto_kem_dec_ref(k3.data(), ct0.data(), sk0.data()) == -1);
    ASSERT_FALSE(k2.IsZero());
    ASSERT_FALSE(k3.IsZero());
    ASSERT_EQ(k2, k3);
    ASSERT_EQ(k0, k3);
#endif
  }

}  // namespace llarp
