#include <exit/context.hpp>

#include <config/config.hpp>
#include <router/router.hpp>
#include <exit/context.hpp>
#include "config/config.hpp"

#include <gtest/gtest.h>

struct ExitTest : public ::testing::Test
{
  ExitTest() : r(nullptr, nullptr, nullptr), context(&r)
  {
  }

  llarp::Router r;
  llarp::exit::Context context;
};

TEST_F(ExitTest, AddMultipleIP)
{
  llarp::PubKey pk;
  pk.Randomize();
  llarp::PathID_t firstPath, secondPath;
  firstPath.Randomize();
  secondPath.Randomize();

  // TODO: exit and type
  // llarp::exit::Context::Config_t conf;
  // conf.emplace("exit", "true");
  // conf.emplace("type", "null");

  llarp::NetworkConfig networkConfig;
  networkConfig.m_ifaddr = "10.0.0.1/24";

  ASSERT_TRUE(context.AddExitEndpoint("test-exit", networkConfig, {}));
  ASSERT_TRUE(context.ObtainNewExit(pk, firstPath, true));
  ASSERT_TRUE(context.ObtainNewExit(pk, secondPath, true));
  ASSERT_TRUE(context.FindEndpointForPath(firstPath)->LocalIP()
              == context.FindEndpointForPath(secondPath)->LocalIP());
}
