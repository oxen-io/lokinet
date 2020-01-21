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

TEST_F(LogLevelTest, TestLogLevelToName)
{
  EXPECT_EQ("Trace", LogLevelToName(llarp::eLogTrace));
  EXPECT_EQ("Debug", LogLevelToName(llarp::eLogDebug));
  EXPECT_EQ("Info", LogLevelToName(llarp::eLogInfo));
  EXPECT_EQ("Warn", LogLevelToName(llarp::eLogWarn));
  EXPECT_EQ("Error", LogLevelToName(llarp::eLogError));
  EXPECT_EQ("None", LogLevelToName(llarp::eLogNone));
  EXPECT_EQ("???", LogLevelToName( (llarp::LogLevel)99999 ));
}

TEST_F(LogLevelTest, TestLogLevelToString)
{
  EXPECT_EQ("TRC", LogLevelToString(llarp::eLogTrace));
  EXPECT_EQ("DBG", LogLevelToString(llarp::eLogDebug));
  EXPECT_EQ("NFO", LogLevelToString(llarp::eLogInfo));
  EXPECT_EQ("WRN", LogLevelToString(llarp::eLogWarn));
  EXPECT_EQ("ERR", LogLevelToString(llarp::eLogError));
  EXPECT_EQ("???", LogLevelToString(llarp::eLogNone));
}

