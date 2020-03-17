#include <config/definition.hpp>

#include <catch2/catch.hpp>

TEST_CASE("ConfigDefinition parse test", "[config]")
{
  llarp::ConfigDefinition<int> def("foo", "bar", false, false, 42);

  CHECK(def.getValue() == 42);
  CHECK(def.numFound == 0);

  CHECK(def.defaultValueAsString() == "42");

  CHECK_NOTHROW(def.parseValue("43"));
  CHECK(def.getValue() == 43);
  CHECK(def.numFound == 1);

  CHECK(def.defaultValueAsString() == "42");
}

TEST_CASE("ConfigDefinition multiple parses test", "[config]")
{
  {
    llarp::ConfigDefinition<int> def("foo", "bar", false, true, 8);

    CHECK_NOTHROW(def.parseValue("9"));
    CHECK(def.getValue() == 9);
    CHECK(def.numFound == 1);

    // should allow since it is multi-value
    CHECK_NOTHROW(def.parseValue("12"));
    CHECK(def.getValue() == 12);
    CHECK(def.numFound == 2);
  }

  {
    llarp::ConfigDefinition<int> def("foo", "baz", false, false, 4);

    CHECK_NOTHROW(def.parseValue("3"));
    CHECK(def.getValue() == 3);
    CHECK(def.numFound == 1);

    // shouldn't allow since not multi-value
    CHECK_THROWS(def.parseValue("2"));
    CHECK(def.numFound == 1);
  }

}

TEST_CASE("Configuration basic add/get test", "[config]")
{
  llarp::Configuration config;
  config.addDefinition(std::make_unique<llarp::ConfigDefinition<int>>(
            "router",
            "threads",
            false,
            false,
            4));

  CHECK(config.getConfigValue<int>("router", "threads") == 4);

  CHECK_NOTHROW(config.addConfigValue(
        "router",
        "threads",
        "5"));

  CHECK(config.getConfigValue<int>("router", "threads") == 5);
}

TEST_CASE("Configuration required test", "[config]")
{
  llarp::Configuration config;
  config.addDefinition(std::make_unique<llarp::ConfigDefinition<int>>(
            "router",
            "threads",
            true,
            false,
            1));

  CHECK_THROWS(config.validate());

  config.addConfigValue("router", "threads", "12");

  CHECK_NOTHROW(config.validate());
}
