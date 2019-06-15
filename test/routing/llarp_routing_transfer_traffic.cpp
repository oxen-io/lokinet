#include <gtest/gtest.h>
#include <routing/transfer_traffic.hpp>

using TransferTrafficMessage = llarp::routing::TransferTrafficMessage;

class TransferTrafficTest : public ::testing::Test
{
};

TEST_F(TransferTrafficTest, TestPutBufferOverflow)
{
  TransferTrafficMessage msg;
  std::array< byte_t, llarp::routing::MaxExitMTU* 2 > tmp = {{0}};
  llarp_buffer_t buf(tmp);
  ASSERT_FALSE(msg.PutBuffer(buf, 1));
}

TEST_F(TransferTrafficTest, TestPutBuffer)
{
  TransferTrafficMessage msg;
  std::array< byte_t, llarp::routing::MaxExitMTU > tmp = {{0}};
  llarp_buffer_t buf(tmp);
  ASSERT_TRUE(msg.PutBuffer(buf, 1));
}
