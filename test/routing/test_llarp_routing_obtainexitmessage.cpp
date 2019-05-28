#include <messages/exit.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>

#include <gtest/gtest.h>

using ObtainExitMessage = llarp::routing::ObtainExitMessage;

class ObtainExitTest : public ::testing::Test
{
 public:
  llarp::sodium::CryptoLibSodium crypto;
  llarp::CryptoManager cm;
  llarp::SecretKey alice;

  ObtainExitTest() : cm(&crypto)
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
  EXPECT_TRUE(msg.Sign(alice));
  EXPECT_TRUE(msg.Verify());
  EXPECT_TRUE(msg.I == llarp::PubKey(llarp::seckey_topublic(alice)));
  EXPECT_FALSE(msg.version != LLARP_PROTO_VERSION);
  EXPECT_FALSE(msg.Z.IsZero());
}
