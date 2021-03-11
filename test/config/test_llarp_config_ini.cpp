#include <config/ini.hpp>

#include <catch2/catch.hpp>

TEST_CASE("ConfigParser", "[config]")
{
  llarp::ConfigParser parser;

  SECTION("Parse empty")
  {
    REQUIRE(parser.LoadFromStr(""));
  }

  SECTION("Parse one section")
  {
    llarp::ConfigParser::SectionValues_t sect;
    // this is an anti pattern don't write this kind of code with configpaser
    auto assertVisit = [&sect](const auto& section) -> bool {
      sect = section;
      return true;
    };
    REQUIRE(parser.LoadFromStr("[test]\nkey=val   \n"));
    REQUIRE(parser.VisitSection("test", assertVisit));
    auto itr = sect.find("notfound");
    REQUIRE(itr == sect.end());
    itr = sect.find("key");
    REQUIRE(itr != sect.end());
    REQUIRE(itr->second == "val");
  }

  SECTION("Parse section duplicate keys")
  {
    REQUIRE(parser.LoadFromStr("[test]\nkey1=val1\nkey1=val2"));
    size_t num = 0;
    auto visit = [&num](const auto& section) -> bool {
      num = section.count("key1");
      return true;
    };
    REQUIRE(parser.VisitSection("test", visit));
    REQUIRE(num == size_t(2));
  }

  SECTION("No key")
  {
    REQUIRE_FALSE(parser.LoadFromStr("[test]\n=1090\n"));
  }

  SECTION("Parse invalid")
  {
    REQUIRE_FALSE(
        parser.LoadFromStr("srged5ghe5\nf34wtge5\nw34tgfs4ygsd5yg=4;\n#"
                           "g4syhgd5\n"));
  }

  parser.Clear();
}
