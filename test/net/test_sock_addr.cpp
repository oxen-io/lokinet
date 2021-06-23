#include <util/mem.hpp>
#include <net/sock_addr.hpp>
#include <net/net_if.hpp>
#include <util/logging/logger.hpp>

#include <catch2/catch.hpp>

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
  CHECK(llarp::SockAddr("5.6.7.8", llarp::huint16_t{5678}).toString() == "5.6.7.8:5678");

  CHECK_THROWS_WITH(llarp::SockAddr("abcd"), "abcd is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("0.0.0.0:foo"), "foo is not a valid port");

  CHECK_THROWS_WITH(llarp::SockAddr("256.257.258.259"), "256.257.258.259 contains invalid numeric value");

  CHECK_THROWS_WITH(llarp::SockAddr("-1.-2.-3.-4"), "-1.-2.-3.-4 contains invalid numeric value");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3"), "1.2.3 is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3."), "1.2.3. is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr(".1.2.3"), ".1.2.3 is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3.4.5"), "1.2.3.4.5 is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3. "), "1.2.3.  contains invalid numeric value");

  CHECK_THROWS_WITH(llarp::SockAddr("1a.2b.3c.4z"), "1a.2b.3c.4z contains invalid numeric value");

  // TODO: there's no reason this couldn't be supported
  CHECK_THROWS_WITH(
      llarp::SockAddr("0xFF.0xFF.0xFF.0xFF"), "0xFF.0xFF.0xFF.0xFF contains invalid numeric value");

  // This *is* supported now; it gives you an empty address (same as default constructed).
  //CHECK_THROWS_WITH(llarp::SockAddr(""), "cannot construct IPv4 from empty string");

  CHECK_THROWS_WITH(llarp::SockAddr(" "), "  is not a valid IPv4 address");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3.4:65536"), "65536 is not a valid port");

  CHECK_THROWS_WITH(llarp::SockAddr("1.2.3.4:1a"), "1a is not a valid port");

  CHECK_THROWS_WITH(llarp::SockAddr("5.6.7.8:1234", llarp::huint16_t{5678}), "invalid ip address (port not allowed here): 5.6.7.8:1234");
}

TEST_CASE("SockAddr from sockaddr_in", "[SockAddr]")
{
  sockaddr_in sin4;
  llarp::Zero(&sin4, sizeof(sockaddr_in));
  sin4.sin_family = AF_INET;
  sin4.sin_addr.s_addr = inet_addr("127.0.0.1");
  sin4.sin_port = htons(1234);

  llarp::SockAddr addr(sin4);

  CHECK(addr.toString() == "127.0.0.1:1234");
}

TEST_CASE("SockAddr from sockaddr_in6", "[SockAddr]")
{
  sockaddr_in6 sin6;
  llarp::Zero(&sin6, sizeof(sockaddr_in6));
  sin6.sin6_family = AF_INET6;
  inet_pton(AF_INET6, "::ffff:127.0.0.1", &sin6.sin6_addr);

  sin6.sin6_port = htons(53);

  llarp::SockAddr addr(sin6);

  CHECK(addr.toString() == "127.0.0.1:53");
}
