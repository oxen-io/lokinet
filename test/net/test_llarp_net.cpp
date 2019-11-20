#include <gtest/gtest.h>

#include <net/net.hpp>
#include <net/net_int.hpp>
#include <net/ip.hpp>

struct TestNet : public ::testing::Test
{
};


TEST_F(TestNet, TestIn6AddrFromString)
{
  llarp::huint128_t ip;
  ASSERT_TRUE(ip.FromString("fc00::1"));
}

TEST_F(TestNet, TestIn6AddrFromStringFail)
{
  llarp::huint128_t ip;
  ASSERT_FALSE(ip.FromString("10.1.1.1"));
}

TEST_F(TestNet, TestIn6AddrToHUIntLoopback)
{
  llarp::huint128_t loopback = {0};
  ASSERT_TRUE(loopback.FromString("::1"));
  in6_addr addr = IN6ADDR_LOOPBACK_INIT;
  auto huint = llarp::net::IPPacket::In6ToHUInt(addr);
  ASSERT_EQ(huint, loopback);
}

TEST_F(TestNet, TestIn6AddrToHUInt)
{
  llarp::huint128_t huint_parsed = {0};
  ASSERT_TRUE(huint_parsed.FromString("fd00::1"));
  in6_addr addr = { { { 0xfd, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01 } } };
  auto huint = llarp::net::IPPacket::In6ToHUInt(addr);
  ASSERT_EQ(huint, huint_parsed);
  huint_parsed.h ++;
  ASSERT_NE(huint, huint_parsed);
}


TEST_F(TestNet, TestRangeContains8)
{
  ASSERT_TRUE(llarp::iprange_ipv4(10, 0, 0, 1, 8)
                  .ContainsV4(llarp::ipaddr_ipv4_bits(10, 40, 11, 6)));
}

TEST_F(TestNet, TestRangeContains24)
{
  ASSERT_TRUE(llarp::iprange_ipv4(10, 200, 0, 1, 24)
                  .ContainsV4(llarp::ipaddr_ipv4_bits(10, 200, 0, 253)));
}

TEST_F(TestNet, TestRangeContainsFail)
{
  ASSERT_TRUE(!llarp::iprange_ipv4(192, 168, 0, 1, 24)
                   .ContainsV4(llarp::ipaddr_ipv4_bits(10, 200, 0, 253)));
}

TEST_F(TestNet, TestIPv4Netmask)
{
  ASSERT_TRUE(llarp::netmask_ipv4_bits(8) == llarp::huint32_t{0xFF000000});
  ASSERT_TRUE(llarp::netmask_ipv4_bits(24) == llarp::huint32_t{0xFFFFFF00});
}

TEST_F(TestNet, TestBogon_10_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(10, 40, 11, 6)));
}

TEST_F(TestNet, TestBogon_192_168_16)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(192, 168, 1, 111)));
}

TEST_F(TestNet, TestBogon_DoD_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(21, 3, 37, 70)));
}

TEST_F(TestNet, TestBogon_127_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(127, 0, 0, 1)));
}

TEST_F(TestNet, TestBogon_0_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(0, 0, 0, 0)));
}

TEST_F(TestNet, TestBogon_NonBogon)
{
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(1, 1, 1, 1)));
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(8, 8, 6, 6)));
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(141, 55, 12, 99)));
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(79, 12, 3, 4)));
}
