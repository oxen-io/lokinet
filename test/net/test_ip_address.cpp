#include <net/ip_address.hpp>

#include <catch2/catch.hpp>

TEST_CASE("IpAddress empty constructor", "[IpAdress]")
{
  llarp::IpAddress address;
  CHECK(address.isEmpty() == true);
}
