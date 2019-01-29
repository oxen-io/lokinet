#include <crypto/encrypted_frame.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <messages/relay_commit.hpp>

#include <gtest/gtest.h>

using EncryptedFrame = llarp::EncryptedFrame;
using SecretKey      = llarp::SecretKey;
using PubKey         = llarp::PubKey;
using LRCR           = llarp::LR_CommitRecord;

class FrameTest : public ::testing::Test
{
 public:
  llarp::sodium::CryptoLibSodium crypto;
  SecretKey alice, bob;

  FrameTest()
  {
  }

  ~FrameTest()
  {
  }

  void
  SetUp()
  {
    crypto.encryption_keygen(alice);
    crypto.encryption_keygen(bob);
  }

  void
  TearDown()
  {
  }
};

TEST_F(FrameTest, TestFrameCrypto)
{
  EncryptedFrame f(256);
  f.Fill(0);
  LRCR record;
  record.nextHop.Fill(1);
  record.tunnelNonce.Fill(2);
  record.rxid.Fill(3);
  record.txid.Fill(4);

  auto buf = f.Buffer();
  buf->cur = buf->base + llarp::EncryptedFrameOverheadSize;

  ASSERT_TRUE(record.BEncode(buf));

  // rewind buffer
  buf->cur = buf->base + llarp::EncryptedFrameOverheadSize;
  // encrypt to alice
  ASSERT_TRUE(f.EncryptInPlace(alice, bob.toPublic(), &crypto));
  // decrypt from alice
  ASSERT_TRUE(f.DecryptInPlace(bob, &crypto));

  LRCR otherRecord;
  ASSERT_TRUE(otherRecord.BDecode(buf));
  ASSERT_TRUE(otherRecord == record);
};
