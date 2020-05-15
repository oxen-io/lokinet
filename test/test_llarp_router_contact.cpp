#include <gtest/gtest.h>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>
#include <router_contact.hpp>
#include <net/net_int.hpp>

using namespace ::llarp;
using namespace ::testing;

static const byte_t DEF_VALUE[] = "unittest";

struct RCTest : public test::LlarpTest<>
{
  using RC_t = RouterContact;
  using SecKey_t = SecretKey;

  RCTest() : oldval(NetID::DefaultValue())
  {
    NetID::DefaultValue() = NetID(DEF_VALUE);
  }

  ~RCTest()
  {
    NetID::DefaultValue() = oldval;
  }

  const NetID oldval;
};

TEST_F(RCTest, TestSignVerify)
{
  NetID netid(DEF_VALUE);
  RC_t rc;
  SecKey_t encr;
  SecKey_t sign;

  rc.enckey = encr.toPublic();
  rc.pubkey = sign.toPublic();
  rc.exits.emplace_back(rc.pubkey, IpAddress("1.1.1.1"));
  ASSERT_TRUE(rc.netID == netid);
  ASSERT_TRUE(rc.netID == NetID::DefaultValue());

  EXPECT_CALL(m_crypto, sign(_, sign, _)).WillOnce(Return(true));
  EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));

  ASSERT_TRUE(rc.Sign(sign));
  ASSERT_TRUE(rc.Verify(time_now_ms()));
}
