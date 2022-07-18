#include <catch2/catch.hpp>
#include <util/logging.hpp>
#include <config/config.hpp>

using TestString = std::string;

struct TestParseLog
{
  TestString input;
  std::optional<llarp::log::Level> level;
};

std::vector<TestParseLog> testParseLog{// bad cases
                                       {"bogus", {}},
                                       {"BOGUS", {}},
                                       {"", {}},
                                       {" ", {}},
                                       {"infogarbage", {}},
                                       {"notcritical", {}},
                                       // good cases
                                       {"info", llarp::log::Level::info},
                                       {"infO", llarp::log::Level::info},
                                       {"iNfO", llarp::log::Level::info},
                                       {"InfO", llarp::log::Level::info},
                                       {"INFO", llarp::log::Level::info},
                                       {"trace", llarp::log::Level::trace},
                                       {"debug", llarp::log::Level::debug},
                                       {"warn", llarp::log::Level::warn},
                                       {"warning", llarp::log::Level::warn},
                                       {"error", llarp::log::Level::err},
                                       {"err", llarp::log::Level::err},
                                       {"Critical", llarp::log::Level::critical},
                                       {"off", llarp::log::Level::off},
                                       {"none", llarp::log::Level::off}};

TEST_CASE("parseLevel")
{
  const auto data = GENERATE(from_range(testParseLog));
  const auto maybe = llarp::log::level_from_string(data.input);
  CHECK(maybe == data.level);
}

TEST_CASE("TestLogLevelToString")
{
  CHECK("trace" == llarp::log::to_string(llarp::log::Level::trace));
  CHECK("debug" == llarp::log::to_string(llarp::log::Level::debug));
  CHECK("info" == llarp::log::to_string(llarp::log::Level::info));
  CHECK("warning" == llarp::log::to_string(llarp::log::Level::warn));
  CHECK("error" == llarp::log::to_string(llarp::log::Level::err));
  CHECK("critical" == llarp::log::to_string(llarp::log::Level::critical));
  CHECK("off" == llarp::log::to_string(llarp::log::Level::off));
}
