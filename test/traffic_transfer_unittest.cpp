#include <gtest/gtest.h>
#include <messages/transfer_traffic.hpp>

using TransferTrafficMessage = llarp::routing::TransferTrafficMessage;

class TransferTrafficTest : public ::testing::Test
{
};

TEST_F(TransferTrafficTest, TestPutBufferOverflow)
{
  TransferTrafficMessage msg;
  byte_t tmp[llarp::routing::MaxExitMTU * 2] = {0};
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  ASSERT_FALSE(msg.PutBuffer(buf, 1));
};

TEST_F(TransferTrafficTest, TestPutBuffer)
{
  TransferTrafficMessage msg;
  byte_t tmp[llarp::routing::MaxExitMTU] = {0};
  auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
  ASSERT_TRUE(msg.PutBuffer(buf, 1));
};
