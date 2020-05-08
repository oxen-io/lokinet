#include <net/sock_addr.hpp>
#include <net/ip_address.hpp>
#include <net/net_if.hpp>

#include <catch2/catch.hpp>

TEST_CASE("SockAddr from sockaddr", "[SockAddr]")
{
  sockaddr sa;

  // check that this compiles (taking sockaddr by ref)
  CHECK_NOTHROW(llarp::SockAddr(sa));

  sockaddr* saptr = &sa;

  // check that this compiles (taking sockaddr by ref from cast from ptr)
  // this was giving odd compilation errors from within net/net.cpp
  llarp::SockAddr addr(*saptr);

  llarp::IpAddress ip(addr);
}

TEST_CASE("SockAddr from IPv4", "[SockAddr]")
{
  llarp::SockAddr addr(1, 2, 3, 4);
  CHECK(addr.toString() == "1.2.3.4:0");
}

TEST_CASE("SockAddr test port", "[SockAddr]")
{
  llarp::SockAddr addr;
  addr.setPort(42);
  CHECK(addr.getPort() == 42);
}

TEST_CASE("SockAddr fromString", "[SockAddr]")
{
  llarp::SockAddr addr;
  CHECK_NOTHROW(addr.fromString("1.2.3.4"));
  CHECK(addr.toString() == "1.2.3.4:0");
}
