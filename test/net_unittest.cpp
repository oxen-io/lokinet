#include <gtest/gtest.h>
#include <llarp/net.hpp>
#include <llarp/net_inaddr.hpp>

struct NetTest : public ::testing::Test
{
};

TEST_F(NetTest, TestinAddrFrom4Int)
{
  llarp::inAddr test;
  test.from4int(127, 0, 0, 1);
  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &test._addr, str, INET6_ADDRSTRLEN) == NULL) {
    ASSERT_TRUE(false);
  }
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}

TEST_F(NetTest, TestinAddrFromStr)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &test._addr, str, INET6_ADDRSTRLEN) == NULL) {
    ASSERT_TRUE(false);
  }
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}

TEST_F(NetTest, TestinAddrReset)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  test.reset();
  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &test._addr, str, INET6_ADDRSTRLEN) == NULL) {
    ASSERT_TRUE(false);
  }
  ASSERT_TRUE(strcmp("0.0.0.0", str) == 0);
}

TEST_F(NetTest, TestinAddrModeSet)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  //test.hexDebug();
  ASSERT_TRUE(test.isIPv4Mode());

  // corrupt it
  test._addr.s6_addr[10] = 0xfe;
  test._addr.s6_addr[11] = 0xfe;

  test.setIPv4Mode();
  //test.hexDebug();
  ASSERT_TRUE(test.isIPv4Mode());
}

TEST_F(NetTest, TestinAddrSIIT)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");

  test.toSIIT();
  //test.hexDebug();
  ASSERT_TRUE(llarp::ipv6_is_siit(test._addr));

  test.fromSIIT();
  //test.hexDebug();
  ASSERT_TRUE(!llarp::ipv6_is_siit(test._addr));
}


TEST_F(NetTest, TestinAddrN32)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  llarp::nuint32_t netOrder = test.toN32();
  llarp::inAddr test2;
  test2.fromN32(netOrder);
  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &test2._addr, str, INET6_ADDRSTRLEN) == NULL) {
    ASSERT_TRUE(false);
  }
  //printf("[%s]\n", str);
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}

TEST_F(NetTest, TestinAddrH32)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  llarp::huint32_t netOrder = test.toH32();
  llarp::inAddr test2;
  test2.fromH32(netOrder);
  char str[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &test2._addr, str, INET6_ADDRSTRLEN) == NULL) {
    ASSERT_TRUE(false);
  }
  //printf("[%s]\n", str);
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}


TEST_F(NetTest, TestRangeContains8)
{
  ASSERT_TRUE(llarp::iprange_ipv4(10, 0, 0, 1, 8)
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
  ASSERT_TRUE(llarp::netmask_ipv4_bits(8)
              == llarp::huint32_t{0xFF000000});
  ASSERT_TRUE(llarp::netmask_ipv4_bits(24)
              == llarp::huint32_t{0xFFFFFF00});
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
