#include <gtest/gtest.h>
#include <llarp/messages/exit.hpp>

using ObtainExitMessage = llarp::routing::ObtainExitMessage;

class ObtainExitTest : public ::testing::Test
{
 public:
  llarp_crypto crypto;
  llarp::SecretKey alice;

  ObtainExitTest()
  {
    llarp_crypto_init(&crypto);
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
  msg.S = llarp_randint();
  msg.T = llarp_randint();
  ASSERT_TRUE(msg.Sign(&crypto, alice));
  ASSERT_TRUE(msg.Verify(&crypto));
  ASSERT_TRUE(msg.I == llarp::PubKey(llarp::seckey_topublic(alice)));
  ASSERT_FALSE(msg.version != LLARP_PROTO_VERSION);
  ASSERT_FALSE(msg.Z.IsZero());
};