#include <crypto/encrypted_frame.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>
#include <messages/relay_commit.hpp>

#include <test_util.hpp>

#include <catch2/catch.hpp>

using namespace ::llarp;

using EncryptedFrame = EncryptedFrame;
using SecretKey = SecretKey;
using PubKey = PubKey;
using LRCR = LR_CommitRecord;

class FrameTest : public test::LlarpTest<>
{
 public:
  FrameTest() : test::LlarpTest<>{}
  {
    auto crypto = CryptoManager::instance();
    crypto->encryption_keygen(alice);
    crypto->encryption_keygen(bob);
  }

  SecretKey alice, bob;
};

TEST_CASE_METHOD(FrameTest, "Frame crypto")
{
  EncryptedFrame f{256};
  f.Fill(0);
  LRCR record{};
  record.nextHop.Fill(1);
  record.tunnelNonce.Fill(2);
  record.rxid.Fill(3);
  record.txid.Fill(4);

  auto buf = f.Buffer();
  buf->cur = buf->base + EncryptedFrameOverheadSize;

  REQUIRE(record.BEncode(buf));

  // rewind buffer
  buf->cur = buf->base + EncryptedFrameOverheadSize;
  // encrypt to alice
  REQUIRE(f.EncryptInPlace(alice, bob.toPublic()));

  // decrypt from alice
  REQUIRE(f.DecryptInPlace(bob));

  LRCR otherRecord;
  REQUIRE(otherRecord.BDecode(buf));
  REQUIRE(otherRecord == record);
}
