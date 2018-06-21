#include <gtest/gtest.h>
#include <llarp/crypto.hpp>
#include <llarp/encrypted_frame.hpp>
#include <llarp/messages/relay_commit.hpp>

using EncryptedFrame = llarp::EncryptedFrame;
using SecretKey      = llarp::SecretKey;
using PubKey         = llarp::PubKey;
using LRAR           = llarp::LR_AcceptRecord;

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
  LRAR record;
  record.upstream.Fill(1);
  record.downstream.Fill(2);
  record.pathid.Fill(3);

  auto buf = f.Buffer();
  buf->cur = buf->base + EncryptedFrame::OverheadSize;

  ASSERT_TRUE(record.BEncode(buf));

  buf->cur = buf->base;
  // encrypt alice to bob
  ASSERT_TRUE(f.EncryptInPlace(alice, llarp::seckey_topublic(bob), &crypto));
  ASSERT_TRUE(f.DecryptInPlace(bob, &crypto));
};