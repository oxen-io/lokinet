#include <gtest/gtest.h>
#include <llarp/messages/transfer_traffic.hpp>

using TransferTrafficMessage = llarp::routing::TransferTrafficMessage;

class TransferTrafficTest : public ::testing::Test
{
 public:
  llarp_crypto crypto;
  llarp::SecretKey alice;

  TransferTrafficTest()
  {
    llarp_crypto_init(&crypto);
  }

  ~TransferTrafficTest()
  {
  }

  void
  SetUp()
  {
    crypto.identity_keygen(alice);
  }
};

TEST_F(TransferTrafficTest, TestSignVerify)
{
  TransferTrafficMessage msg;
  msg.X.resize(1024);
  msg.S = 100;
  crypto.randbytes(msg.X.data(), 1024);
  ASSERT_TRUE(msg.Sign(&crypto, alice));
  ASSERT_FALSE(msg.Z.IsZero());
  ASSERT_TRUE(msg.Verify(&crypto, llarp::seckey_topublic(alice)));
};

TEST_F(TransferTrafficTest, TestPutBufferOverflow)
{
  TransferTrafficMessage msg;
  byte_t tmp[llarp::routing::MaxExitMTU * 2] = {0};
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  ASSERT_FALSE(msg.PutBuffer(buf));
};

TEST_F(TransferTrafficTest, TestPutBuffer)
{
  TransferTrafficMessage msg;
  byte_t tmp[llarp::routing::MaxExitMTU] = {0};
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  ASSERT_TRUE(msg.PutBuffer(buf));
};
