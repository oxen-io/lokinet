#include <gtest/gtest.h>

#include <messages/exit.hpp>

using ObtainExitMessage = llarp::routing::ObtainExitMessage;

class ObtainExitTest : public ::testing::Test
{
 public:
     llarp::Crypto crypto;
  llarp::SecretKey alice;

  ObtainExitTest()
  : crypto(llarp::Crypto::sodium{})
  {
  }

  ~ObtainExitTest()
  {
  }

  void
  SetUp()
  {
    crypto.identity_keygen(alice);
  }
};

TEST_F(ObtainExitTest, TestSignVerify)
{
  ObtainExitMessage msg;
  msg.Z.Zero();
  msg.S = llarp::randint();
  msg.T = llarp::randint();
  ASSERT_TRUE(msg.Sign(&crypto, alice));
  ASSERT_TRUE(msg.Verify(&crypto));
  ASSERT_TRUE(msg.I == llarp::PubKey(llarp::seckey_topublic(alice)));
  ASSERT_FALSE(msg.version != LLARP_PROTO_VERSION);
  ASSERT_FALSE(msg.Z.IsZero());
};
