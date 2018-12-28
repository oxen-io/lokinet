#include <gtest/gtest.h>

#include <router_contact.hpp>
#include <crypto.hpp>

struct RCTest : public ::testing::Test
{
  using RC_t     = llarp::RouterContact;
  using SecKey_t = llarp::SecretKey;

  RCTest() : crypto(llarp::Crypto::sodium{})
  {
  }

  void
  SetUp()
  {
    oldval                     = llarp::NetID::DefaultValue;
    llarp::NetID::DefaultValue = (const byte_t*)"unittest";
    rc.Clear();
  }

  void
  TearDown()
  {
    llarp::NetID::DefaultValue = oldval;
  }

  RC_t rc;
  llarp::Crypto crypto;
  const byte_t* oldval = nullptr;
};

TEST_F(RCTest, TestSignVerify)
{
  SecKey_t encr;
  SecKey_t sign;
  crypto.encryption_keygen(encr);
  crypto.identity_keygen(sign);
  rc.enckey = llarp::seckey_topublic(encr);
  ASSERT_TRUE(rc.Sign(&crypto, sign));
  ASSERT_TRUE(rc.Verify(&crypto, llarp::time_now_ms()));
}
