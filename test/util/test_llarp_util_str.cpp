#include <util/str.hpp>
#include <catch2/catch.hpp>

#include <vector>

using namespace std::literals;

TEST_CASE("TrimWhitespace -- positive tests", "[str][trim]")
{
  // Test that things that should be trimmed actually get trimmed
  auto fee = "    J a c k"s;
  auto fi = "\ra\nd"s;
  auto fo = "\fthe   "s;
  auto fum = " \t\r\n\v\f Beanstalk\n\n\n\t\r\f\v   \n\n\r\f\f\f\f\v"s;
  for (auto* s: {&fee, &fi, &fo, &fum})
    *s = llarp::TrimWhitespace(*s);

  REQUIRE( fee == "J a c k" );
  REQUIRE( fi == "a\nd" );
  REQUIRE( fo == "the" );
  REQUIRE( fum == "Beanstalk" );
}

TEST_CASE("TrimWhitespace -- negative tests", "[str][trim]")
{
  // Test that things that shouldn't be trimmed don't get trimmed
  auto c = GENERATE(range(std::numeric_limits<char>::min(), std::numeric_limits<char>::max()));
  std::string plant = c + "bean"s + c;
  plant = llarp::TrimWhitespace(plant);
  if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')
    REQUIRE( plant == "bean" );
  else
  {
    REQUIRE( plant.size() == 6 );
    REQUIRE( plant.substr(1, 4) == "bean" );
  }
}

TEST_CASE("caseless comparison tests - less than", "[str][lt]") {
  using namespace llarp;
  CaselessLessThan lt;
  auto expect_less_than = GENERATE(table<const char*, const char*>({
        {"", "1"},
        {"1", "11"},
        {"abc", "abcd"},
        {"ABC", "abcd"},
        {"abc", "ABCD"},
        {"abc", "Abcd"},
        {"abc", "abcD"},
        {"abc", "abCd"},
        {"abc", "zz"},
        {"abc", "zzzz"},
        {"abc", "abd"},
        {"abc", "aBd"},
        {"abc", "abD"},
        {"ABC", "abd"},
        {"abC", "abd"},
  }));
  REQUIRE(  lt(std::get<0>(expect_less_than), std::get<1>(expect_less_than)) );
  REQUIRE( !lt(std::get<1>(expect_less_than), std::get<0>(expect_less_than)) );
}

TEST_CASE("caseless comparison tests - equality", "[str][eq]") {
  using namespace llarp;
  CaselessLessThan lt;
  auto expect_equal = GENERATE(table<const char*, const char*>({
        {"1", "1"},
        {"a", "A"},
        {"abc", "ABC"},
        {"abc", "aBc"},
        {"ABC", "abc"},
  }));
  REQUIRE( !lt(std::get<0>(expect_equal), std::get<1>(expect_equal)) );
  REQUIRE( !lt(std::get<1>(expect_equal), std::get<0>(expect_equal)) );
}

TEST_CASE("truthy string values", "[str][truthy]") {
  auto val = GENERATE("true", "TruE", "yes", "yeS", "yES", "yes", "YES", "1", "on", "oN", "ON");
  REQUIRE( llarp::IsTrueValue(val) );
}

TEST_CASE("falsey string values", "[str][falsey]") {
  auto val = GENERATE("false", "FalSe", "no", "NO", "No", "nO", "0", "off", "OFF");
  REQUIRE( llarp::IsFalseValue(val) );
}

TEST_CASE("neither true nor false string values", "[str][nottruefalse]") {
  auto val = GENERATE("false y", "maybe", "not on", "2", "yesno", "YESNO", "-1", "default", "OMG");
  REQUIRE( !llarp::IsTrueValue(val) );
  REQUIRE( !llarp::IsFalseValue(val) );
}

TEST_CASE("split strings with multiple matches", "[str]") {
  auto splits = llarp::split("this is a test", ' ');
  REQUIRE(splits.size() == 4);
  REQUIRE(splits[0] == "this");
  REQUIRE(splits[1] == "is");
  REQUIRE(splits[2] == "a");
  REQUIRE(splits[3] == "test");
}

TEST_CASE("split strings with single match", "[str]") {
  auto splits = llarp::split("uno", ';');
  REQUIRE(splits.size() == 1);
  REQUIRE(splits[0] == "uno");
}

TEST_CASE("split strings with consecutive delimiters", "[str]") {
  auto splits = llarp::split("a  o   e    u", ' ');
  REQUIRE(splits.size() == 4);
  REQUIRE(splits[0] == "a");
  REQUIRE(splits[1] == "o");
  REQUIRE(splits[2] == "e");
  REQUIRE(splits[3] == "u");
}

TEST_CASE("split delimiter-only string", "[str]") {
  auto splits = llarp::split("    ", ' ');
  REQUIRE(splits.size() == 0);

  splits = llarp::split(" ", ' ');
  REQUIRE(splits.size() == 0);
}

TEST_CASE("split empty string", "[str]") {
  auto splits = llarp::split("", ' ');
  REQUIRE(splits.size() == 0);
}
