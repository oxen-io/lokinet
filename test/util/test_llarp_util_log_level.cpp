#include <gtest/gtest.h>
#include <util/logging/loglevel.hpp>

using TestString = std::string;

struct TestParseLog
{
  TestString input;
  absl::optional< llarp::LogLevel > level;
};

struct LogLevelTest : public ::testing::TestWithParam< TestParseLog >
{
};

TEST_P(LogLevelTest, parseLevel)
{
  const auto data  = GetParam();
  const auto maybe = llarp::LogLevelFromString(data.input);
  EXPECT_EQ(maybe, data.level);
}

static const TestParseLog testParseLog[] = {
    // bad cases
    {"bogus", {}},
    {"BOGUS", {}},
    {"", {}},
    {" ", {}},
    // good cases
    {"info", llarp::eLogInfo},
    {"infO", llarp::eLogInfo},
    {"iNfO", llarp::eLogInfo},
    {"InfO", llarp::eLogInfo},
    {"INFO", llarp::eLogInfo},
    {"debug", llarp::eLogDebug},
    {"warn", llarp::eLogWarn},
    {"error", llarp::eLogError},
    {"none", llarp::eLogNone}};

INSTANTIATE_TEST_CASE_P(TestLogConfig, LogLevelTest,
                        ::testing::ValuesIn(testParseLog), );
