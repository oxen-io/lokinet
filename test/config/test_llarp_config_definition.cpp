#include <config/definition.hpp>

#include <catch2/catch.hpp>

TEST_CASE("ConfigDefinition int parse test", "[config]")
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

TEST_CASE("ConfigDefinition string parse test", "[config]")
{
  llarp::ConfigDefinition<std::string> def("foo", "bar", false, false, "test");

  CHECK(def.getValue() == "test");
  CHECK(def.defaultValueAsString() == "test");

  CHECK_NOTHROW(def.parseValue("foo"));
  CHECK(def.getValue() == "foo");
  CHECK(def.numFound == 1);
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

TEST_CASE("ConfigDefinition acceptor test", "[config]")
{
  int test = -1;
  llarp::ConfigDefinition<int> def("foo", "bar", false, false, 42, [&](int arg) {
    test = arg;
  });

  CHECK_NOTHROW(def.tryAccept());
  CHECK(test == 42);

  def.parseValue("43");
  CHECK_NOTHROW(def.tryAccept());
  CHECK(test == 43);
}

TEST_CASE("ConfigDefinition acceptor throws test", "[config]")
{
  llarp::ConfigDefinition<int> def("foo", "bar", false, false, 42, [&](int arg) {
    (void)arg;
    throw std::runtime_error("FAIL");
  });

  REQUIRE_THROWS_WITH(def.tryAccept(), "FAIL");
}

TEST_CASE("ConfigDefinition tryAccept missing option test", "[config]")
{
  int unset = -1;
  llarp::ConfigDefinition<int> def("foo", "bar", true, false, 1, [&](int arg) {
    (void)arg;
    unset = 0; // should never be called
  });

  REQUIRE_THROWS_WITH(def.tryAccept(),
      "cannot call tryAccept() on [foo]:bar when required but no value available");
}

TEST_CASE("Configuration basic add/get test", "[config]")
{
  llarp::Configuration config;
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
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

TEST_CASE("Configuration missing def test", "[config]")
{
  llarp::Configuration config;
  CHECK_THROWS(config.addConfigValue("foo", "bar", "5"));
  CHECK_THROWS(config.getConfigValue<int>("foo", "bar") == 5);

  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "quux",
            "bar",
            false,
            false,
            4));

  CHECK_THROWS(config.addConfigValue("foo", "bar", "5"));
}

TEST_CASE("Configuration required test", "[config]")
{
  llarp::Configuration config;
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "router",
            "threads",
            true,
            false,
            1));

  CHECK_THROWS(config.validateRequiredFields());

  config.addConfigValue("router", "threads", "12");

  CHECK_NOTHROW(config.validateRequiredFields());
}

TEST_CASE("Configuration section test", "[config]")
{
  llarp::Configuration config;
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "foo",
            "bar",
            true,
            false,
            1));
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
            "goo",
            "bar",
            true,
            false,
            1));

  CHECK_THROWS(config.validateRequiredFields());

  config.addConfigValue("foo", "bar", "5");
  CHECK_THROWS(config.validateRequiredFields());

  CHECK_NOTHROW(config.addConfigValue("goo", "bar", "6"));
  CHECK_NOTHROW(config.validateRequiredFields());

  CHECK(config.getConfigValue<int>("foo", "bar") == 5);
  CHECK(config.getConfigValue<int>("goo", "bar") == 6);
}

TEST_CASE("Configuration acceptAllOptions test", "[config]")
{
  int fooBar = -1;
  std::string fooBaz = "";

  llarp::Configuration config;
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
      "foo", "bar", false, false, 1, [&](int arg) {
        fooBar = arg;
      }));
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<std::string>>(
      "foo", "baz", false, false, "no", [&](std::string arg) {
        fooBaz = arg;
      }));

  config.addConfigValue("foo", "baz", "yes");

  REQUIRE_NOTHROW(config.validateRequiredFields());
  REQUIRE_NOTHROW(config.acceptAllOptions());
  CHECK(fooBar == 1);
  CHECK(fooBaz == "yes");
}

TEST_CASE("Configuration acceptAllOptions exception propagation test", "[config]")
{
  llarp::Configuration config;
  config.addConfigOption(std::make_unique<llarp::ConfigDefinition<int>>(
      "foo", "bar", false, false, 1, [&](int arg) {
        (void)arg;
        throw std::runtime_error("FAIL");
      }));

  REQUIRE_THROWS_WITH(config.acceptAllOptions(), "FAIL");
}
