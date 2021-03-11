#include <exit/exit_messages.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>

#include <catch2/catch.hpp>

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
  SecretKey alice{};
  CryptoManager::instance()->identity_keygen(alice);
  REQUIRE(not alice.IsZero());
  ObtainExitMessage msg{};
  msg.S = randint();
  msg.T = randint();
  CHECK(msg.Sign(alice));
  CHECK(msg.Verify());
  CHECK(msg.I == PubKey{seckey_topublic(alice)});
  CHECK(msg.version == LLARP_PROTO_VERSION);
  CHECK_FALSE(msg.Z.IsZero());
}
