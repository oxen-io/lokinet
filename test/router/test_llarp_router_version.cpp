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


