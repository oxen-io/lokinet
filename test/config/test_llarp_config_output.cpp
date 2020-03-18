#include <config/definition.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Configuration simple generate test", "[config]")
{
  llarp::Configuration config;

  config.defineOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "foo", "bar", true, 1));
  config.defineOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "foo", "baz", false, 2));
  config.defineOption(std::make_unique<llarp::ConfigDefinition<std::string>>(
            "foo", "quux", true, "hello"));

  config.defineOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "argle", "bar", true, 3));
  config.defineOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "argle", "baz", false, 4));
  config.defineOption(std::make_unique<llarp::ConfigDefinition<std::string>>(
            "argle", "quux", true, "the quick brown fox"));

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw([foo]
bar=1
# baz=2
quux=hello

[argle]
bar=3
# baz=4
quux=the quick brown fox
)raw");
}

TEST_CASE("Configuration useValue test", "[config]")
{
  llarp::Configuration config;

  config.defineOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "foo", "bar", true, 1));

  constexpr auto expected = "[foo]\nbar=1\n";

  CHECK(config.generateINIConfig(false) == expected);
  CHECK(config.generateINIConfig(true) == expected);

  config.addConfigValue("foo", "bar", "2");

  constexpr auto expectedWhenValueProvided = "[foo]\nbar=2\n";

  CHECK(config.generateINIConfig(false) == expected);
  CHECK(config.generateINIConfig(true) == expectedWhenValueProvided);
}

