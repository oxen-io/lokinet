#include <messages/exit.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>

#include <gtest/gtest.h>

using ObtainExitMessage = llarp::routing::ObtainExitMessage;

class ObtainExitTest : public ::testing::Test
{
 public:
  llarp::sodium::CryptoLibSodium crypto;
  llarp::SecretKey alice;

  ObtainExitTest()
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
  EXPECT_TRUE(msg.Sign(&crypto, alice));
  EXPECT_TRUE(msg.Verify(&crypto));
  EXPECT_TRUE(msg.I == llarp::PubKey(llarp::seckey_topublic(alice)));
  EXPECT_FALSE(msg.version != LLARP_PROTO_VERSION);
  EXPECT_FALSE(msg.Z.IsZero());
}
