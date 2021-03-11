#include <routing/transfer_traffic_message.hpp>

#include <catch2/catch.hpp>

using TransferTrafficMessage = llarp::routing::TransferTrafficMessage;

TEST_CASE("TransferTrafficMessage", "[TransferTrafficMessage]")
{
  TransferTrafficMessage msg;

  SECTION("Put buffer overflow")
  {
    std::array<byte_t, llarp::routing::MaxExitMTU* 2> tmp = {{0}};
    llarp_buffer_t buf(tmp);
    REQUIRE_FALSE(msg.PutBuffer(buf, 1));
  }

  SECTION("Put buffer")
  {
    std::array<byte_t, llarp::routing::MaxExitMTU> tmp = {{0}};
    llarp_buffer_t buf(tmp);
    REQUIRE(msg.PutBuffer(buf, 1));
  }
}
