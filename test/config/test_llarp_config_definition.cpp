#include <config/definition.hpp>

#include <catch2/catch.hpp>

using namespace llarp::config;

TEST_CASE("OptionDefinition int parse test", "[config]")
{
  llarp::OptionDefinition<int> def("foo", "bar", Default{42});

  CHECK(def.getValue() == 42);
  CHECK(def.getNumberFound() == 0);

  CHECK(def.defaultValueAsString() == "42");

  CHECK_NOTHROW(def.parseValue("43"));
  CHECK(def.getValue() == 43);
  CHECK(def.getNumberFound() == 1);

  CHECK(def.defaultValueAsString() == "42");

  constexpr Default sqrt_625{25};
  llarp::OptionDefinition<int> def2("a", "b", sqrt_625);
  CHECK(def2.getValue() == 25);
  CHECK(def2.defaultValueAsString() == "25");

  CHECK_NOTHROW(def2.parseValue("99"));
  CHECK(def2.getValue() == 99);
  CHECK(def2.getNumberFound() == 1);
}

TEST_CASE("OptionDefinition string parse test", "[config]")
{
  llarp::OptionDefinition<std::string> def("foo", "bar", Default{"test"});

  CHECK(def.getValue() == "test");
  CHECK(def.defaultValueAsString() == "test");

  CHECK_NOTHROW(def.parseValue("foo"));
  CHECK(def.getValue() == "foo");
  CHECK(def.getNumberFound() == 1);
}

TEST_CASE("OptionDefinition multiple parses test", "[config]")
{
  {
    llarp::OptionDefinition<int> def("foo", "bar", MultiValue, Default{8});

    CHECK_NOTHROW(def.parseValue("9"));
    CHECK(def.getValue() == 9);
    CHECK(def.getNumberFound() == 1);

    // should allow since it is multi-value
    CHECK_NOTHROW(def.parseValue("12"));
    CHECK(def.getValue() == 9); // getValue() should return first value
    REQUIRE(def.getNumberFound() == 2);

  }

  {
    llarp::OptionDefinition<int> def("foo", "baz", Default{4});

    CHECK_NOTHROW(def.parseValue("3"));
    CHECK(def.getValue() == 3);
    CHECK(def.getNumberFound() == 1);

    // shouldn't allow since not multi-value
    CHECK_THROWS(def.parseValue("2"));
    CHECK(def.getNumberFound() == 1);
  }

}

TEST_CASE("OptionDefinition acceptor test", "[config]")
{
  int test = -1;
  llarp::OptionDefinition<int> def("foo", "bar", Default{42}, [&](int arg) {
    test = arg;
  });

  CHECK_NOTHROW(def.tryAccept());
  CHECK(def.getValue() == 42);
  CHECK(def.defaultValue);
  CHECK(*def.defaultValue == 42);
  CHECK(test == 42);

  def.parseValue("43");
  CHECK_NOTHROW(def.tryAccept());
  CHECK(test == 43);
}

TEST_CASE("OptionDefinition acceptor throws test", "[config]")
{
  llarp::OptionDefinition<int> def("foo", "bar", Default{42}, [&](int arg) {
    (void)arg;
    throw std::runtime_error("FAIL");
  });

  REQUIRE_THROWS_WITH(def.tryAccept(), "FAIL");
}

TEST_CASE("OptionDefinition tryAccept missing option test", "[config]")
{
  int unset = -1;
  llarp::OptionDefinition<int> def("foo", "bar", Required, Default{1}, [&](int arg) {
    (void)arg;
    unset = 0; // should never be called
  });

  REQUIRE_THROWS_WITH(def.tryAccept(),
      "cannot call tryAccept() on [foo]:bar when required but no value available");
}

TEST_CASE("ConfigDefinition basic add/get test", "[config]")
{
  llarp::ConfigDefinition config{true};
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "router",
            "threads",
            Default{4}));

  CHECK(config.getConfigValue<int>("router", "threads") == 4);

  CHECK_NOTHROW(config.addConfigValue(
        "router",
        "threads",
        "5"));

  CHECK(config.getConfigValue<int>("router", "threads") == 5);
}

TEST_CASE("ConfigDefinition router/client-only tests", "[config]")
{
  llarp::ConfigDefinition r_config{true}, c_config{false};
  r_config.defineOption<int>("router", "abc", Default{1}, RelayOnly);
  r_config.defineOption<int>("router", "def", Default{1}, ClientOnly);
  r_config.defineOption<int>("router", "ghi", Default{1});
  c_config.defineOption<int>("router", "abc", Default{1}, RelayOnly);
  c_config.defineOption<int>("router", "def", Default{1}, ClientOnly);
  c_config.defineOption<int>("router", "ghi", Default{1});

  CHECK_NOTHROW(r_config.getConfigValue<int>("router", "abc"));
  CHECK_THROWS(r_config.getConfigValue<int>("router", "def"));
  CHECK_NOTHROW(r_config.getConfigValue<int>("router", "ghi"));

  CHECK_THROWS(c_config.getConfigValue<int>("router", "abc"));
  CHECK_NOTHROW(c_config.getConfigValue<int>("router", "def"));
  CHECK_NOTHROW(c_config.getConfigValue<int>("router", "ghi"));
}

TEST_CASE("ConfigDefinition missing def test", "[config]")
{
  llarp::ConfigDefinition config{true};
  CHECK_THROWS(config.addConfigValue("foo", "bar", "5"));
  CHECK_THROWS(config.getConfigValue<int>("foo", "bar") == 5);

  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "quux",
            "bar",
            Default{4}));

  CHECK_THROWS(config.addConfigValue("foo", "bar", "5"));
}

TEST_CASE("ConfigDefinition required test", "[config]")
{
  llarp::ConfigDefinition config{true};
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "router",
            "threads",
            Default{1},
            Required));

  CHECK_THROWS(config.validateRequiredFields());

  config.addConfigValue("router", "threads", "12");

  CHECK_NOTHROW(config.validateRequiredFields());
}

TEST_CASE("ConfigDefinition section test", "[config]")
{
  llarp::ConfigDefinition config{true};
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "foo",
            "bar",
            Required,
            Default{1}));
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
            "goo",
            "bar",
            Required,
            Default{1}));

  CHECK_THROWS(config.validateRequiredFields());

  config.addConfigValue("foo", "bar", "5");
  CHECK_THROWS(config.validateRequiredFields());

  CHECK_NOTHROW(config.addConfigValue("goo", "bar", "6"));
  CHECK_NOTHROW(config.validateRequiredFields());

  CHECK(config.getConfigValue<int>("foo", "bar") == 5);
  CHECK(config.getConfigValue<int>("goo", "bar") == 6);
}

TEST_CASE("ConfigDefinition acceptAllOptions test", "[config]")
{
  int fooBar = -1;
  std::string fooBaz = "";

  llarp::ConfigDefinition config{true};
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
      "foo", "bar", Default{1}, [&](int arg) {
        fooBar = arg;
      }));
  config.defineOption(std::make_unique<llarp::OptionDefinition<std::string>>(
      "foo", "baz", Default{"no"}, [&](std::string arg) {
        fooBaz = arg;
      }));

  config.addConfigValue("foo", "baz", "yes");

  REQUIRE_NOTHROW(config.validateRequiredFields());
  REQUIRE_NOTHROW(config.acceptAllOptions());
  CHECK(fooBar == 1);
  CHECK(fooBaz == "yes");
}

TEST_CASE("ConfigDefinition acceptAllOptions exception propagation test", "[config]")
{
  llarp::ConfigDefinition config{true};
  config.defineOption(std::make_unique<llarp::OptionDefinition<int>>(
      "foo", "bar", Default{1}, [&](int arg) {
        (void)arg;
        throw std::runtime_error("FAIL");
      }));

  REQUIRE_THROWS_WITH(config.acceptAllOptions(), "FAIL");
}

TEST_CASE("ConfigDefinition defineOptions passthrough test", "[config]")
{
  llarp::ConfigDefinition config{true};
  config.defineOption<int>("foo", "bar", Default{1});
  CHECK(config.getConfigValue<int>("foo", "bar") == 1);
}

TEST_CASE("ConfigDefinition undeclared definition basic test", "[config]")
{
  llarp::ConfigDefinition config{true};

  bool invoked = false;

  config.addUndeclaredHandler("foo", [&](std::string_view section, std::string_view name, std::string_view value) {
    CHECK(section == "foo");
    CHECK(name == "bar");
    CHECK(value == "val");

    invoked = true;
  });

  REQUIRE_NOTHROW(config.addConfigValue("foo", "bar", "val"));

  CHECK(invoked);
}

TEST_CASE("ConfigDefinition undeclared add more than once test", "[config]")
{
  llarp::ConfigDefinition config{true};

  std::string calledBy = "";

  config.addUndeclaredHandler("foo", [&](std::string_view, std::string_view, std::string_view) {
      calledBy = "a";
  });
  REQUIRE_THROWS_WITH(
    config.addUndeclaredHandler("foo", [&](std::string_view, std::string_view, std::string_view) {
        calledBy = "b";
    }),
    "section foo already has a handler");

  REQUIRE_NOTHROW(config.addConfigValue("foo", "bar", "val"));

  CHECK(calledBy == "a");
}

TEST_CASE("ConfigDefinition undeclared add/remove test", "[config]")
{
  llarp::ConfigDefinition config{true};

  std::string calledBy = "";

  // add...
  REQUIRE_NOTHROW(config.addUndeclaredHandler("foo", [&](std::string_view, std::string_view, std::string_view) {
    calledBy = "a";
  }));

  REQUIRE_NOTHROW(config.addConfigValue("foo", "bar", "val"));

  CHECK(calledBy == "a");

  calledBy = "";

  // ...then remove...
  REQUIRE_NOTHROW(config.removeUndeclaredHandler("foo"));

  CHECK_THROWS_WITH(config.addConfigValue("foo", "bar", "val"), "unrecognized section [foo]");

  // ...then add again
  REQUIRE_NOTHROW(config.addUndeclaredHandler("foo", [&](std::string_view, std::string_view, std::string_view) {
    calledBy = "b";
  }));

  REQUIRE_NOTHROW(config.addConfigValue("foo", "bar", "val"));

  CHECK(calledBy == "b");
}

TEST_CASE("ConfigDefinition undeclared handler exception propagation test", "[config]")
{
  llarp::ConfigDefinition config{true};

  config.addUndeclaredHandler("foo", [](std::string_view, std::string_view, std::string_view) {
      throw std::runtime_error("FAIL");
  });

  REQUIRE_THROWS_WITH(config.addConfigValue("foo", "bar", "val"), "FAIL");
}

TEST_CASE("ConfigDefinition undeclared handler wrong section", "[config]")
{
  llarp::ConfigDefinition config{true};

  config.addUndeclaredHandler("foo", [](std::string_view, std::string_view, std::string_view) {
      throw std::runtime_error("FAIL");
  });

  REQUIRE_THROWS_WITH(config.addConfigValue("argle", "bar", "val"), "unrecognized section [argle]");
}

TEST_CASE("ConfigDefinition undeclared handler duplicate names", "[config]")
{
  llarp::ConfigDefinition config{true};

  int count = 0;

  config.addUndeclaredHandler("foo", [&](std::string_view, std::string_view, std::string_view) {
      count++;
  });

  REQUIRE_NOTHROW(config.addConfigValue("foo", "k", "v"));
  REQUIRE_NOTHROW(config.addConfigValue("foo", "k", "v"));
  REQUIRE_NOTHROW(config.addConfigValue("foo", "k", "v"));

  REQUIRE(count == 3);
}

TEST_CASE("ConfigDefinition AssignmentAcceptor", "[config]")
{
  llarp::ConfigDefinition config{true};

  int val = -1;
  config.defineOption<int>("foo", "bar", Default{2}, AssignmentAcceptor(val));

  config.addConfigValue("foo", "bar", "3");
  CHECK_NOTHROW(config.acceptAllOptions());

  REQUIRE(val == 3);
}

TEST_CASE("ConfigDefinition multiple values", "[config]")
{
  llarp::ConfigDefinition config{true};

  std::vector<int> values;
  config.defineOption<int>("foo", "bar", MultiValue, Default{2}, [&](int arg) {
    values.push_back(arg);
  });

  config.addConfigValue("foo", "bar", "1");
  config.addConfigValue("foo", "bar", "2");
  config.addConfigValue("foo", "bar", "3");
  CHECK_NOTHROW(config.acceptAllOptions());

  REQUIRE(values.size() == 3);
  CHECK(values[0] == 1);
  CHECK(values[1] == 2);
  CHECK(values[2] == 3);
}

TEST_CASE("ConfigDefinition [bind]iface regression", "[config regression]")
{
  llarp::ConfigDefinition config{true};

  std::string val1;
  std::string undeclaredName;
  std::string undeclaredValue;

  config.defineOption<std::string>(
      "bind", "*", Default{"1090"}, [&](std::string arg) { val1 = arg; });

  config.addUndeclaredHandler("bind", [&](std::string_view, std::string_view name, std::string_view value) {
    undeclaredName = std::string(name);
    undeclaredValue = std::string(value);
  });

  config.addConfigValue("bind", "enp35s0", "1091");
  CHECK_NOTHROW(config.acceptAllOptions());

  CHECK(val1 == "1090");
  CHECK(undeclaredName == "enp35s0");
  CHECK(undeclaredValue == "1091");
}

TEST_CASE("ConfigDefinition truthy/falsy bool values", "[config]")
{
  // truthy values
  for (auto val : {"true", "on", "yes", "1"})
  {
    llarp::OptionDefinition<bool> def("foo", "bar", Default{false});

    // defaults to false
    auto maybe = def.getValue();
    REQUIRE(maybe);
    CHECK(*maybe == false);

    // val should result in true
    CHECK_NOTHROW(def.parseValue(val));
    maybe = def.getValue();
    REQUIRE(maybe);
    CHECK(*maybe);
  }

  // falsy values
  for (auto val : {"false", "off", "no", "0"})
  {
    llarp::OptionDefinition<bool> def("foo", "bar", Default{true});

    // defaults to true
    auto maybe = def.getValue();
    REQUIRE(maybe);
    CHECK(maybe == true);

    // val should result in false
    CHECK_NOTHROW(def.parseValue(val));
    maybe = def.getValue();
    REQUIRE(maybe);
    CHECK(maybe == false);
  }

  // illegal values
  for (auto val : {"", " ", "TRUE", "argle", " false", "2"})
  {
    llarp::OptionDefinition<bool> def("foo", "bar", Default{true});
    CHECK_THROWS(def.parseValue(val));
  }
}
