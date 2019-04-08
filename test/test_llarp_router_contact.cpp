#include <gtest/gtest.h>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <router_contact.hpp>

static const byte_t DEF_VALUE[] = "unittest";

struct RCTest : public ::testing::Test
{
  using RC_t     = llarp::RouterContact;
  using SecKey_t = llarp::SecretKey;

  RCTest() : oldval(llarp::NetID::DefaultValue())
  {
    llarp::NetID::DefaultValue() = llarp::NetID(DEF_VALUE);
  }

  ~RCTest()
  {
    llarp::NetID::DefaultValue() = oldval;
  }

  llarp::sodium::CryptoLibSodium crypto;
  const llarp::NetID oldval;
};

TEST_F(RCTest, TestSignVerify)
{
  llarp::NetID netid(DEF_VALUE);
  RC_t rc;
  SecKey_t encr;
  SecKey_t sign;

  crypto.encryption_keygen(encr);
  crypto.identity_keygen(sign);
  rc.enckey = encr.toPublic();
  rc.pubkey = sign.toPublic();
  rc.exits.emplace_back(rc.pubkey, llarp::nuint32_t{50000});
  ASSERT_TRUE(rc.netID == netid);
  ASSERT_TRUE(rc.netID == llarp::NetID::DefaultValue());
  ASSERT_TRUE(rc.Sign(&crypto, sign));
  ASSERT_TRUE(rc.Verify(&crypto, llarp::time_now_ms()));
}
