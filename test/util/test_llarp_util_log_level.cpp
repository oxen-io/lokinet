#include <gtest/gtest.h>
#include <util/logging/loglevel.hpp>
#include <util/logging/logger.hpp>
#include <config/config.hpp>

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

INSTANTIATE_TEST_SUITE_P(TestLogConfig, LogLevelTest,
                         ::testing::ValuesIn(testParseLog));

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

TEST_F(LogLevelTest, TestLoggingConfigSideEffects)
{
  // restore original runtime level when we're done
  llarp::LogContext& logContext = llarp::LogContext::Instance();
  auto original = logContext.runtimeLevel;

  // LoggingConfig::fromSection updates the runtime level as it reads in conf
  // values, so feed it values and ensure that the runtime level is updated
  // appropriately
  llarp::LoggingConfig config;

  config.fromSection("level", "Trace");
  EXPECT_EQ(llarp::eLogTrace, logContext.runtimeLevel);

  config.fromSection("level", "Debug");
  EXPECT_EQ(llarp::eLogDebug, logContext.runtimeLevel);

  config.fromSection("level", "Info");
  EXPECT_EQ(llarp::eLogInfo, logContext.runtimeLevel);

  config.fromSection("level", "Warn");
  EXPECT_EQ(llarp::eLogWarn, logContext.runtimeLevel);

  config.fromSection("level", "Error");
  EXPECT_EQ(llarp::eLogError, logContext.runtimeLevel);

  config.fromSection("level", "None");
  EXPECT_EQ(llarp::eLogNone, logContext.runtimeLevel);


  SetLogLevel(original);
}

