#include <catch2/catch.hpp>
#include <util/logging/loglevel.hpp>
#include <util/logging/logger.hpp>
#include <config/config.hpp>

using TestString = std::string;

struct TestParseLog
{
  TestString input;
  std::optional<llarp::LogLevel> level;
};

std::vector<TestParseLog> testParseLog{// bad cases
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

TEST_CASE("parseLevel")
{
  const auto data = GENERATE(from_range(testParseLog));
  const auto maybe = llarp::LogLevelFromString(data.input);
  CHECK(maybe == data.level);
}

TEST_CASE("TestLogLevelToName")
{
  CHECK("Trace" == LogLevelToName(llarp::eLogTrace));
  CHECK("Debug" == LogLevelToName(llarp::eLogDebug));
  CHECK("Info" == LogLevelToName(llarp::eLogInfo));
  CHECK("Warn" == LogLevelToName(llarp::eLogWarn));
  CHECK("Error" == LogLevelToName(llarp::eLogError));
  CHECK("None" == LogLevelToName(llarp::eLogNone));
  CHECK("???" == LogLevelToName((llarp::LogLevel)99999));
}

TEST_CASE("TestLogLevelToString")
{
  CHECK("TRC" == LogLevelToString(llarp::eLogTrace));
  CHECK("DBG" == LogLevelToString(llarp::eLogDebug));
  CHECK("NFO" == LogLevelToString(llarp::eLogInfo));
  CHECK("WRN" == LogLevelToString(llarp::eLogWarn));
  CHECK("ERR" == LogLevelToString(llarp::eLogError));
  CHECK("???" == LogLevelToString(llarp::eLogNone));
}
