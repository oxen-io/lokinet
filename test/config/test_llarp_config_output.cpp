#include <config/definition.hpp>

#include <catch2/catch.hpp>

TEST_CASE("ConfigDefinition simple generate test", "[config]")
{
  llarp::ConfigDefinition config;

  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "bar", true, 1));
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "baz", false, 2));
  config.defineOption(std::make_unique<llarp::OptionDefinition<std::string>>(
            "foo", "quux", true, "hello"));

  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "argle", "bar", true, 3));
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "argle", "baz", false, 4));
  config.defineOption(std::make_unique<llarp::OptionDefinition<std::string>>(
            "argle", "quux", true, "the quick brown fox"));

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw([foo]

bar=1

#baz=2

quux=hello


[argle]

bar=3

#baz=4

quux=the quick brown fox
)raw");
}

TEST_CASE("ConfigDefinition useValue test", "[config]")
{
  llarp::ConfigDefinition config;

  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "bar", true, 1));

  constexpr auto expected = "[foo]\n\nbar=1\n";

  CHECK(config.generateINIConfig(false) == expected);
  CHECK(config.generateINIConfig(true) == expected);

  config.addConfigValue("foo", "bar", "2");

  constexpr auto expectedWhenValueProvided = "[foo]\n\nbar=2\n";

  CHECK(config.generateINIConfig(false) == expected);
  CHECK(config.generateINIConfig(true) == expectedWhenValueProvided);
}

TEST_CASE("ConfigDefinition section comments test")
{
  llarp::ConfigDefinition config;

  config.addSectionComment("foo", "test comment");
  config.addSectionComment("foo", "test comment 2");
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "bar", true, 1));

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw(# test comment
# test comment 2
[foo]

bar=1
)raw");
}

TEST_CASE("ConfigDefinition option comments test")
{
  llarp::ConfigDefinition config;

  config.addOptionComment("foo", "bar", "test comment 1");
  config.addOptionComment("foo", "bar", "test comment 2");
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "bar", true, 1));

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw([foo]

# test comment 1
# test comment 2
bar=1
)raw");
}

TEST_CASE("ConfigDefinition empty comments test")
{
  llarp::ConfigDefinition config;

  config.addSectionComment("foo", "section comment");
  config.addSectionComment("foo", "");

  config.addOptionComment("foo", "bar", "option comment");
  config.addOptionComment("foo", "bar", "");
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "bar", true, 1));

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw(# section comment
# 
[foo]

# option comment
# 
bar=1
)raw");
}

TEST_CASE("ConfigDefinition multi option comments")
{
  llarp::ConfigDefinition config;

  config.addSectionComment("foo", "foo section comment");

  config.addOptionComment("foo", "bar", "foo bar option comment");
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "bar", true, 1));

  config.addOptionComment("foo", "baz", "foo baz option comment");
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo", "baz", true, 1));

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw(# foo section comment
[foo]

# foo bar option comment
bar=1

# foo baz option comment
baz=1
)raw");
}

TEST_CASE("ConfigDefinition should print comments for missing keys")
{
  // TODO: this currently fails: how to implement?
  llarp::ConfigDefinition config;

  config.addSectionComment("foo", "foo section comment");
  config.addOptionComment("foo", "bar", "foo bar option comment");

  std::string output = config.generateINIConfig();

  CHECK(output == R"raw(# foo section comment
[foo]

# foo bar option comment
bar=
)raw");
}
