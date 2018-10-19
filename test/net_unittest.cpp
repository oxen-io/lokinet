#include <gtest/gtest.h>
#include <llarp/net.hpp>

struct NetTest : public ::testing::Test
{
};

TEST_F(NetTest, TestRangeContains8)
{
  ASSERT_TRUE(llarp::iprange_ipv4(10, 0, 0, 0, 8)
                  .Contains(llarp::ipaddr_ipv4_bits(10, 40, 11, 6)));
}

TEST_F(NetTest, TestRangeContains24)
{
  ASSERT_TRUE(llarp::iprange_ipv4(10, 200, 0, 1, 24)
                  .Contains(llarp::ipaddr_ipv4_bits(10, 200, 0, 253)));
}

TEST_F(NetTest, TestRangeContainsFail)
{
  ASSERT_TRUE(!llarp::iprange_ipv4(192, 168, 0, 1, 24)
                  .Contains(llarp::ipaddr_ipv4_bits(10, 200, 0, 253)));
}


TEST_F(NetTest, TestIPv4Netmask)
{
  ASSERT_TRUE(llarp::xhtonl(llarp::netmask_ipv4_bits(8))
              == llarp::nuint32_t{0xFF000000});
};

TEST_F(NetTest, TestBogon_10_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(10, 40, 11, 6)));
};

TEST_F(NetTest, TestBogon_192_168_16)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(192, 168, 1, 111)));
};

TEST_F(NetTest, TestBogon_DoD_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(21, 3, 37, 70)));
};

TEST_F(NetTest, TestBogon_127_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(127, 0, 0, 1)));
};

TEST_F(NetTest, TestBogon_0_8)
{
  ASSERT_TRUE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(0, 0, 0, 0)));
};

TEST_F(NetTest, TestBogon_NonBogon)
{
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(1, 1, 1, 1)));
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(8, 8, 6, 6)));
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(141, 55, 12, 99)));
  ASSERT_FALSE(llarp::IsIPv4Bogon(llarp::ipaddr_ipv4_bits(79, 12, 3, 4)));
}
