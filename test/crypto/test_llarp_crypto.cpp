#include <crypto/crypto_libsodium.hpp>

#include <iostream>

#include <catch2/catch.hpp>

using namespace llarp;

TEST_CASE("Identity key")
{
  llarp::sodium::CryptoLibSodium crypto;
  SecretKey secret;
  crypto.identity_keygen(secret);

  SECTION("Keygen")
  {
    REQUIRE_FALSE(secret.IsZero());
  }

  SECTION("Sign-verify")
  {
    AlignedBuffer<128> random;
    random.Randomize();
    Signature sig;
    const PubKey pk = secret.toPublic();

    const llarp_buffer_t buf(random.data(), random.size());
    REQUIRE(crypto.sign(sig, secret, buf));
    REQUIRE(crypto.verify(pk, buf, sig));
    random.Randomize();
    // mangle body
    REQUIRE_FALSE(crypto.verify(pk, buf, sig));
  }
}

TEST_CASE("PQ crypto")
{
  llarp::sodium::CryptoLibSodium crypto;
  PQKeyPair keys;
  crypto.pqe_keygen(keys);
  PQCipherBlock block;
  SharedSecret shared, otherShared;
  auto c = &crypto;

  REQUIRE(keys.size() == PQ_KEYPAIRSIZE);
  REQUIRE(c->pqe_encrypt(block, shared, PQPubKey(pq_keypair_to_public(keys))));
  REQUIRE(c->pqe_decrypt(block, otherShared, pq_keypair_to_secret(keys)));
  REQUIRE(otherShared == shared);
}
