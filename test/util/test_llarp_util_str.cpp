#include <util/str.hpp>
#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("TrimWhiteSpace -- positive tests", "[str][trim]")
{
  // Test that things that should be trimmed actually get trimmed
  auto fee = "    J a c k"s;
  auto fi = "\ra\nd"s;
  auto fo = "\fthe   "s;
  auto fum = " \t\r\n\v\f Beanstalk\n\n\n\t\r\f\v   \n\n\r\f\f\f\f\v"s;
  for (auto* s: {&fee, &fi, &fo, &fum})
    llarp::TrimWhiteSpace(*s);

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
  llarp::TrimWhiteSpace(plant);
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
  auto expect_less_than = GENERATE(table<const char*, const char*>({
        {"1", "1"},
        {"a", "A"},
        {"abc", "ABC"},
        {"abc", "aBc"},
        {"ABC", "abc"},
  }));
  REQUIRE( !lt(std::get<0>(expect_less_than), std::get<1>(expect_less_than)) );
  REQUIRE( !lt(std::get<1>(expect_less_than), std::get<0>(expect_less_than)) );
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
