#include <net/net_int.hpp>
#include <net/ip.hpp>
#include <net/ip_range.hpp>
#include <net/net.hpp>
#include <oxenmq/hex.h>

#include <catch2/catch.hpp>

TEST_CASE("In6Addr")
{
  llarp::huint128_t ip;

  SECTION("From string")
  {
    REQUIRE(ip.FromString("fc00::1"));
  }

  SECTION("From string fail")
  {
    REQUIRE_FALSE(ip.FromString("10.1.1.1"));
  }
}

TEST_CASE("In6AddrToHUIntLoopback")
{
  llarp::huint128_t loopback = {0};
  REQUIRE(loopback.FromString("::1"));
  in6_addr addr = IN6ADDR_LOOPBACK_INIT;
  auto huint = llarp::net::In6ToHUInt(addr);
  REQUIRE(huint == loopback);
}

TEST_CASE("In6AddrToHUInt")
{
  llarp::huint128_t huint_parsed = {0};
  REQUIRE(huint_parsed.FromString("fd00::1"));
  in6_addr addr = {{{0xfd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01}}};
  auto huint = llarp::net::In6ToHUInt(addr);
  REQUIRE(huint == huint_parsed);
  huint_parsed.h++;
  REQUIRE(huint != huint_parsed);
}

TEST_CASE("Range")
{
  SECTION("Contains 8")
  {
    REQUIRE(
        llarp::IPRange::FromIPv4(10, 0, 0, 1, 8).Contains(llarp::ipaddr_ipv4_bits(10, 40, 11, 6)));
  }

  SECTION("Contains 24")
  {
    REQUIRE(llarp::IPRange::FromIPv4(10, 200, 0, 1, 24)
                .Contains(llarp::ipaddr_ipv4_bits(10, 200, 0, 253)));
  }

  SECTION("Contains fail")
  {
    REQUIRE(!llarp::IPRange::FromIPv4(192, 168, 0, 1, 24)
                 .Contains(llarp::ipaddr_ipv4_bits(10, 200, 0, 253)));
  }
}

TEST_CASE("IPv4 netmask")
{
  REQUIRE(llarp::netmask_ipv4_bits(8) == llarp::huint32_t{0xFF000000});
  REQUIRE(llarp::netmask_ipv4_bits(24) == llarp::huint32_t{0xFFFFFF00});
}

TEST_CASE("Bogon")
{
  SECTION("Bogon_10_8")
  {
    REQUIRE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(10, 40, 11, 6)));
  }

  SECTION("Bogon_192_168_16")
  {
    REQUIRE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(192, 168, 1, 111)));
  }

  SECTION("Bogon_127_8")
  {
    REQUIRE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(127, 0, 0, 1)));
  }

  SECTION("Bogon_0_8")
  {
    REQUIRE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(0, 0, 0, 0)));
  }

  SECTION("Non-bogon")
  {
    REQUIRE_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(1, 1, 1, 1)));
    REQUIRE_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(8, 8, 6, 6)));
    REQUIRE_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(141, 55, 12, 99)));
    REQUIRE_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(79, 12, 3, 4)));
  }
}

TEST_CASE("uint128_t")
{
    SECTION("layout")
    {
        llarp::uint128_t i{0x0011223f44556677ULL, 0x8899aabbc3ddeeffULL};
        REQUIRE(oxenmq::to_hex(std::string_view{reinterpret_cast<const char*>(&i), sizeof(i)}) ==
#ifdef __BIG_ENDIAN__
                "0011223f445566778899aabbc3ddeeff"
#else
                "ffeeddc3bbaa9988776655443f221100"
#endif
               );
    }
    SECTION("ntoh")
    {
        llarp::uint128_t i{0x0011223f44556677ULL, 0x8899aabbc3ddeeffULL};
        auto be = ntoh128(i);
        REQUIRE(be == llarp::uint128_t{0xffeeddc3bbaa9988ULL, 0x776655443f221100ULL});
    }
    SECTION("hton")
    {
        llarp::uint128_t i{0x0011223f44556677ULL, 0x8899aabbc3ddeeffULL};
        auto be = ntoh128(i);
        REQUIRE(be == llarp::uint128_t{0xffeeddc3bbaa9988ULL, 0x776655443f221100ULL});
    }
}
