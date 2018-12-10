#include <gtest/gtest.h>
#include <llarp/exit.hpp>
#include <router.hpp>

struct ExitTest : public ::testing::Test
{
  ExitTest() : r(nullptr, nullptr, nullptr)
  {
  }
  llarp::Router r;
};

TEST_F(ExitTest, AddMultipleIP)
{
  llarp::PubKey pk;
  pk.Randomize();
  llarp::PathID_t firstPath, secondPath;
  firstPath.Randomize();
  secondPath.Randomize();
  llarp::exit::Context::Config_t conf;
  conf.emplace("exit", "true");
  conf.emplace("type", "null");
  conf.emplace("ifaddr", "10.0.0.1/24");
  ASSERT_TRUE(r.exitContext.AddExitEndpoint("test-exit", conf));
  ASSERT_TRUE(r.exitContext.ObtainNewExit(pk, firstPath, true));
  ASSERT_TRUE(r.exitContext.ObtainNewExit(pk, secondPath, true));
  ASSERT_TRUE(r.exitContext.FindEndpointForPath(firstPath)->LocalIP()
              == r.exitContext.FindEndpointForPath(secondPath)->LocalIP());
};
