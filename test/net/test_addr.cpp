#include <net/net_addr.hpp>
#include <catch2/catch.hpp>

TEST_CASE("Addr FromString", "[addr]")
{
  llarp::Addr addr;
  bool success = false;
  CHECK_NOTHROW(success = addr.FromString("127.0.0.1:53"));
  CHECK(success == true);

  CHECK(addr.port() == 53);
  CHECK(addr.ToString() == "127.0.0.1:53");
}
