#include <net/net_addr.hpp>
#include <catch2/catch.hpp>

TEST_CASE("Addr from_char_array", "[addr]")
{
  llarp::Addr addr;
  bool success = false;
  CHECK_NOTHROW(success = addr.from_char_array("127.0.0.1:53"));
  CHECK(success == true);
}
