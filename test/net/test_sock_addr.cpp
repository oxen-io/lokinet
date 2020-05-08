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

  CHECK(llarp::SockAddr("1.3.5.7").toString() == "1.3.5.7:0");

  CHECK(llarp::SockAddr("0.0.0.0").toString() == "0.0.0.0:0");
  CHECK(llarp::SockAddr("0.0.0.0:0").toString() == "0.0.0.0:0");
  CHECK(llarp::SockAddr("255.255.255.255").toString() == "255.255.255.255:0");
  CHECK(llarp::SockAddr("255.255.255.255:255").toString() == "255.255.255.255:255");
  CHECK(llarp::SockAddr("255.255.255.255:65535").toString() == "255.255.255.255:65535");

  CHECK_THROWS_WITH(llarp::SockAddr("abcd"), "abcd is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("0.0.0.0:foo"), "0.0.0.0:foo contains invalid port");

  CHECK_THROWS_WITH(llarp::SockAddr("256.257.258.259"), "256.257.258.259 contains invalid number");

  CHECK_THROWS_WITH(llarp::SockAddr("-1.-2.-3.-4"), "-1.-2.-3.-4 contains invalid number");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3"), "1.2.3 is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3."), "1.2.3. is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr(".1.2.3"), ".1.2.3 is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3.4.5"), "1.2.3.4.5 is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3. "), "1.2.3.  contains invalid number");

  CHECK_THROWS_WITH(llarp::SockAddr("1a.2b.3c.4z"), "1a.2b.3c.4z contains non-numeric values");

  // TODO: there's no reason this couldn't be supported
  CHECK_THROWS_WITH(
      llarp::SockAddr("0xFF.0xFF.0xFF.0xFF"), "0xFF.0xFF.0xFF.0xFF contains non-numeric values");

  CHECK_THROWS_WITH(llarp::SockAddr(""), "cannot construct IPv4 from empty string");

  CHECK_THROWS_WITH(llarp::SockAddr(" "), "  is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3.4:65536"), "1.2.3.4:65536 contains invalid port");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3.4:1a"), "1.2.3.4:1a contains junk after port");
}
