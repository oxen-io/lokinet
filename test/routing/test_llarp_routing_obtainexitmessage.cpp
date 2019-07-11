#include <exit/exit_messages.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace ::testing;
using namespace ::llarp;

using ObtainExitMessage = routing::ObtainExitMessage;

class ObtainExitTest : public test::LlarpTest<>
{
 public:
  SecretKey alice;

  ObtainExitTest()
  {
    // m_crypto.identity_keygen(alice);
  }
};

void
fill(Signature& s)
{
  s.Fill(0xFF);
}

TEST_F(ObtainExitTest, TestSignVerify)
{
  EXPECT_CALL(m_crypto, sign(_, alice, _))
      .WillOnce(DoAll(WithArg< 0 >(Invoke(&fill)), Return(true)));
  EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));
  ObtainExitMessage msg;
  msg.Z.Zero();
  msg.S = randint();
  msg.T = randint();
  EXPECT_TRUE(msg.Sign(alice));
  EXPECT_TRUE(msg.Verify());
  EXPECT_TRUE(msg.I == PubKey(seckey_topublic(alice)));
  EXPECT_FALSE(msg.version != LLARP_PROTO_VERSION);
  EXPECT_FALSE(msg.Z.IsZero());
}
