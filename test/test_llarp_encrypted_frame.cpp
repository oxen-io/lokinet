#include <crypto/encrypted_frame.hpp>

#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <llarp_test.hpp>
#include <messages/relay_commit.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace ::llarp;
using namespace ::testing;

using EncryptedFrame = EncryptedFrame;
using SecretKey      = SecretKey;
using PubKey         = PubKey;
using LRCR           = LR_CommitRecord;

class FrameTest : public test::LlarpTest<>
{
 public:
  SecretKey alice, bob;
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
  buf->cur = buf->base + EncryptedFrameOverheadSize;

  ASSERT_TRUE(record.BEncode(buf));

  EXPECT_CALL(m_crypto, randbytes(_, _))
      .WillOnce(Invoke(&test::randbytes_impl));

  EXPECT_CALL(m_crypto, dh_client(_, _, alice, _)).WillOnce(Return(true));
  EXPECT_CALL(m_crypto, xchacha20(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(m_crypto, hmac(_, _, _)).Times(2).WillRepeatedly(Return(true));

  // rewind buffer
  buf->cur = buf->base + EncryptedFrameOverheadSize;
  // encrypt to alice
  ASSERT_TRUE(f.EncryptInPlace(alice, bob.toPublic()));

  EXPECT_CALL(m_crypto, dh_server(_, _, _, _)).WillOnce(Return(true));

  // decrypt from alice
  ASSERT_TRUE(f.DecryptInPlace(bob));

  LRCR otherRecord;
  ASSERT_TRUE(otherRecord.BDecode(buf));
  ASSERT_TRUE(otherRecord == record);
}
