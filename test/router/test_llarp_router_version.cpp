#include <gtest/gtest.h>
#include <router_version.hpp>
#include "router/router.hpp"

class TestRouterVersion : public ::testing::Test
{
};

TEST_F(TestRouterVersion, TestCompatibilityWhenProtocolEqual)
{
  llarp::RouterVersion v1( {0, 1, 2}, 1);
  llarp::RouterVersion v2( {0, 1, 2}, 1);

  EXPECT_TRUE(v1.IsCompatableWith(v2));
}

TEST_F(TestRouterVersion, TestCompatibilityWhenProtocolUnequal)
{
  llarp::RouterVersion older( {0, 1, 2}, 1);
  llarp::RouterVersion newer( {0, 1, 2}, 2);

  EXPECT_FALSE(older.IsCompatableWith(newer));
  EXPECT_FALSE(newer.IsCompatableWith(older));
}

TEST_F(TestRouterVersion, TestEmptyCompatibility)
{
  llarp::RouterVersion v1( {0, 0, 1}, LLARP_PROTO_VERSION);

  EXPECT_FALSE(v1.IsCompatableWith(llarp::emptyRouterVersion));
}

TEST_F(TestRouterVersion, TestIsEmpty)
{
  llarp::RouterVersion notEmpty( {0, 0, 1}, LLARP_PROTO_VERSION);
  EXPECT_FALSE(notEmpty.IsEmpty());

  EXPECT_TRUE(llarp::emptyRouterVersion.IsEmpty());
}

TEST_F(TestRouterVersion, TestClear)
{
  llarp::RouterVersion version( {0, 0, 1}, LLARP_PROTO_VERSION);
  EXPECT_FALSE(version.IsEmpty());

  version.Clear();

  EXPECT_TRUE(version.IsEmpty());
}

TEST_F(TestRouterVersion, TestBEncode)
{
  llarp::RouterVersion v1235( {1, 2, 3}, 5);

  std::array< byte_t, 128 > tmp{};
  llarp_buffer_t buf(tmp);

  EXPECT_TRUE(v1235.BEncode(&buf));

  std::string s((const char*)buf.begin(), (buf.end() - buf.begin()));
  LogInfo("bencoded: ", buf.begin());

  EXPECT_STREQ((const char*)buf.begin(), "li5ei1ei2ei3ee");

}

TEST_F(TestRouterVersion, TestBDecode)
{
  llarp::RouterVersion version;
  version.Clear();

  const std::string bString("li9ei3ei2ei1ee");
  llarp_buffer_t buf(bString.data(), bString.size());
  EXPECT_TRUE(version.BDecode(&buf));

  llarp::RouterVersion expected( {3, 2, 1}, 9);

  EXPECT_EQ(expected, version);

}

TEST_F(TestRouterVersion, TestDecodeLongVersionArray)
{
  llarp::RouterVersion version;
  version.Clear();

  const std::string bString("li9ei3ei2ei1ei2ei3ei4ei5ei6ei7ei8ei9ee");
  llarp_buffer_t buf(bString.data(), bString.size());
  EXPECT_FALSE(version.BDecode(&buf));

}
