#include <gtest/gtest.h>

#include <net_inaddr.hpp>

struct TestNetInAddr : public ::testing::Test
{
};

TEST_F(TestNetInAddr, TestinAddrFrom4Int)
{
  llarp::inAddr test;
  test.from4int(127, 0, 0, 1);
  char str[INET6_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &test._addr, str, INET6_ADDRSTRLEN) == NULL)
  {
    ASSERT_TRUE(false);
  }
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}

TEST_F(TestNetInAddr, TestinAddrFromStr)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  char str[INET6_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &test._addr, str, INET6_ADDRSTRLEN) == NULL)
  {
    ASSERT_TRUE(false);
  }
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}

TEST_F(TestNetInAddr, TestinAddrReset)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  test.reset();
  char str[INET6_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &test._addr, str, INET6_ADDRSTRLEN) == NULL)
  {
    ASSERT_TRUE(false);
  }
  ASSERT_TRUE(strcmp("0.0.0.0", str) == 0);
}

TEST_F(TestNetInAddr, TestinAddrModeSet)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  // test.hexDebug();
  ASSERT_TRUE(test.isIPv4Mode());

  // corrupt it
  test._addr.s6_addr[10] = 0xfe;
  test._addr.s6_addr[11] = 0xfe;

  test.setIPv4Mode();
  // test.hexDebug();
  ASSERT_TRUE(test.isIPv4Mode());
}

TEST_F(TestNetInAddr, TestinAddrSIIT)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");

  test.toSIIT();
  // test.hexDebug();
  ASSERT_TRUE(llarp::ipv6_is_siit(test._addr));

  test.fromSIIT();
  // test.hexDebug();
  ASSERT_TRUE(!llarp::ipv6_is_siit(test._addr));
}

TEST_F(TestNetInAddr, TestinAddrN32)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  llarp::nuint32_t netOrder = test.toN32();
  llarp::inAddr test2;
  test2.fromN32(netOrder);
  char str[INET6_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &test2._addr, str, INET6_ADDRSTRLEN) == NULL)
  {
    ASSERT_TRUE(false);
  }
  // printf("[%s]\n", str);
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}

TEST_F(TestNetInAddr, TestinAddrH32)
{
  llarp::inAddr test;
  test.from_char_array("127.0.0.1");
  llarp::huint32_t netOrder = test.toH32();
  llarp::inAddr test2;
  test2.fromH32(netOrder);
  char str[INET6_ADDRSTRLEN];
  if(inet_ntop(AF_INET, &test2._addr, str, INET6_ADDRSTRLEN) == NULL)
  {
    ASSERT_TRUE(false);
  }
  // printf("[%s]\n", str);
  ASSERT_TRUE(strcmp("127.0.0.1", str) == 0);
}
