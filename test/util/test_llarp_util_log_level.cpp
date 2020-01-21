#include <gtest/gtest.h>
#include <util/logging/loglevel.hpp>

struct LogLevelTest : public ::testing::Test
{
};

TEST_F(LogLevelTest, TestLogLevelNameBadName)
{
  const auto maybe = llarp::LogLevelFromString("bogus");
  ASSERT_FALSE(maybe.has_value());
}
