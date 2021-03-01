#include <exit/exit_messages.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <crypto/mock_crypto.hpp>
#include <llarp_test.hpp>

#include <gmock/gmock.h>

#include <catch2/catch.hpp>

using namespace ::testing;
using namespace ::llarp;
using namespace ::llarp::test;

using ObtainExitMessage = routing::ObtainExitMessage;

void
fill(Signature& s)
{
  s.Fill(0xFF);
}

TEST_CASE_METHOD(LlarpTest<>, "Sign-verify")
{
  SecretKey alice;
  EXPECT_CALL(m_crypto, sign(_, alice, _)).WillOnce(DoAll(WithArg<0>(Invoke(&fill)), Return(true)));
  EXPECT_CALL(m_crypto, verify(_, _, _)).WillOnce(Return(true));
  ObtainExitMessage msg;
  msg.Z.Zero();
  msg.S = randint();
  msg.T = randint();
  CHECK(msg.Sign(alice));
  CHECK(msg.Verify());
  CHECK(msg.I == PubKey(seckey_topublic(alice)));
  CHECK(msg.version == LLARP_PROTO_VERSION);
  CHECK_FALSE(msg.Z.IsZero());
}
