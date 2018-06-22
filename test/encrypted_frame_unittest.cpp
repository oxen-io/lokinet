#include <gtest/gtest.h>
#include <llarp/crypto.hpp>
#include <llarp/encrypted_frame.hpp>
#include <llarp/messages/relay_commit.hpp>

using EncryptedFrame = llarp::EncryptedFrame;
using SecretKey      = llarp::SecretKey;
using PubKey         = llarp::PubKey;
using LRCR           = llarp::LR_CommitRecord;

class FrameTest : public ::testing::Test
{
 public:
  llarp_crypto crypto;
  SecretKey alice, bob;

  FrameTest()
  {
    llarp_crypto_libsodium_init(&crypto);
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
  buf->cur = buf->base + EncryptedFrame::OverheadSize;

  ASSERT_TRUE(record.BEncode(buf));

  // rewind buffer
  buf->cur = buf->base + EncryptedFrame::OverheadSize;
  // encrypt to alice
  ASSERT_TRUE(f.EncryptInPlace(alice, llarp::seckey_topublic(bob), &crypto));
  // decrypt from alice
  ASSERT_TRUE(f.DecryptInPlace(bob, &crypto));

  LRCR otherRecord;
  ASSERT_TRUE(otherRecord.BDecode(buf));
  ASSERT_TRUE(otherRecord == record);
};