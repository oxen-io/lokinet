#include <llarp/config/config.hpp>

#include <catch2/catch.hpp>
#include "mocks/mock_context.hpp"

using namespace std::literals;

struct UnitTestConfigGenParameters : public llarp::ConfigGenParameters
{
  const mocks::Network* const _plat;
  UnitTestConfigGenParameters(const mocks::Network* plat)
      : llarp::ConfigGenParameters{}, _plat{plat}
  {}

  const llarp::net::Platform*
  Net_ptr() const override
  {
    return _plat;
  }
};

struct UnitTestConfig : public llarp::Config
{
  const mocks::Network* const _plat;

  explicit UnitTestConfig(const mocks::Network* plat) : llarp::Config{std::nullopt}, _plat{plat}
  {}

  std::unique_ptr<llarp::ConfigGenParameters>
  MakeGenParams() const override
  {
    return std::make_unique<UnitTestConfigGenParameters>(_plat);
  }
};

std::shared_ptr<UnitTestConfig>
make_config_for_test(const mocks::Network* env, std::string_view ini_str = "")
{
  auto conf = std::make_shared<UnitTestConfig>(env);
  conf->LoadString(ini_str, true);
  conf->lokid.whitelistRouters = false;
  conf->bootstrap.seednode = true;
  conf->bootstrap.files.clear();
  return conf;
}

std::shared_ptr<UnitTestConfig>
make_config(mocks::Network env, std::string_view ini_str = "")
{
  auto conf = std::make_shared<UnitTestConfig>(&env);
  conf->LoadString(ini_str, true);
  conf->lokid.whitelistRouters = false;
  conf->bootstrap.seednode = true;
  conf->bootstrap.files.clear();
  return conf;
}

void
run_config_test(mocks::Network env, std::string_view ini_str)
{
  auto conf = make_config_for_test(&env, ini_str);
  const auto opts = env.Opts();
  auto context = std::make_shared<mocks::MockContext>(env);

  context->Configure(conf);
  context->Setup(opts);
  int ib_links{};
  int ob_links{};

  context->router->linkManager().ForEachInboundLink([&ib_links](auto) { ib_links++; });
  context->router->linkManager().ForEachOutboundLink([&ob_links](auto) { ob_links++; });
  REQUIRE(ib_links == 1);
  REQUIRE(ob_links == 1);
  if (context->Run(opts))
    throw std::runtime_error{"non zero return"};
}

TEST_CASE("service node bind section on valid network", "[config]")
{
  std::unordered_multimap<std::string, llarp::IPRange> env{
      {"mock0", llarp::IPRange::FromIPv4(1, 1, 1, 1, 32)},
      {"lo", llarp::IPRange::FromIPv4(127, 0, 0, 1, 8)},
  };

  SECTION("mock network is sane")
  {
    mocks::Network mock_net{env};
    REQUIRE(mock_net.GetInterfaceAddr("mock0"sv, AF_INET6) == std::nullopt);
    auto maybe_addr = mock_net.GetInterfaceAddr("mock0"sv, AF_INET);
    REQUIRE(maybe_addr != std::nullopt);
    REQUIRE(maybe_addr->hostString() == "1.1.1.1");
    REQUIRE(not mock_net.IsBogon(*maybe_addr));
  }

  SECTION("empty config")
  {
    std::string_view ini_str = "";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }

  SECTION("explicit bind via ifname")
  {
    std::string_view ini_str = R"(
[bind]
mock0=443
)";
    run_config_test(env, ini_str);
  }
  SECTION("explicit bind via ip address")
  {
    std::string_view ini_str = R"(
[bind]
inbound=1.1.1.1:443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("explicit bind via ip address with old syntax")
  {
    std::string_view ini_str = R"(
[bind]
1.1.1.1=443
)";

    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("ip spoof fails")
  {
    std::string_view ini_str = R"(
[router]
public-ip=8.8.8.8
public-port=443
[bind]
inbound=1.1.1.1:443
)";
    REQUIRE_THROWS(run_config_test(env, ini_str));
  }
  SECTION("explicit bind via ifname but fails from non existing ifname")
  {
    std::string_view ini_str = R"(
[bind]
ligma0=443
)";
    REQUIRE_THROWS(make_config(env, ini_str));
  }

  SECTION("explicit bind via ifname but fails from using loopback")
  {
    std::string_view ini_str = R"(
[bind]
lo=443
)";
    REQUIRE_THROWS(make_config(env, ini_str));
  }

  SECTION("explicit bind via explicit loopback")
  {
    std::string_view ini_str = R"(
[bind]
inbound=127.0.0.1:443
)";
    REQUIRE_THROWS(make_config(env, ini_str));
  }
  SECTION("public ip provided but no bind section")
  {
    std::string_view ini_str = R"(
[router]
public-ip=1.1.1.1
public-port=443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("public ip provided with ip in bind section")
  {
    std::string_view ini_str = R"(
[router]
public-ip=1.1.1.1
public-port=443
[bind]
1.1.1.1=443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
}

TEST_CASE("service node bind section on nat network", "[config]")
{
  std::unordered_multimap<std::string, llarp::IPRange> env{
      {"mock0", llarp::IPRange::FromIPv4(10, 1, 1, 1, 32)},
      {"lo", llarp::IPRange::FromIPv4(127, 0, 0, 1, 8)},
  };
  SECTION("no public ip set should fail")
  {
    std::string_view ini_str = "";
    REQUIRE_THROWS(run_config_test(env, ini_str));
  }

  SECTION("public ip provided via inbound directive")
  {
    std::string_view ini_str = R"(
[router]
public-ip=1.1.1.1
public-port=443

[bind]
inbound=10.1.1.1:443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }

  SECTION("public ip provided with bind via ifname")
  {
    std::string_view ini_str = R"(
[router]
public-ip=1.1.1.1
public-port=443

[bind]
mock0=443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }

  SECTION("public ip provided bind via wildcard ip")
  {
    std::string_view ini_str = R"(
[router]
public-ip=1.1.1.1
public-port=443

[bind]
inbound=0.0.0.0:443
)";
    REQUIRE_THROWS(run_config_test(env, ini_str));
  }

}

TEST_CASE("service node bind section with multiple public ip", "[config]")
{
  std::unordered_multimap<std::string, llarp::IPRange> env{
      {"mock0", llarp::IPRange::FromIPv4(1, 1, 1, 1, 32)},
      {"mock0", llarp::IPRange::FromIPv4(2, 1, 1, 1, 32)},
      {"lo", llarp::IPRange::FromIPv4(127, 0, 0, 1, 8)},
  };
  SECTION("empty config")
  {
    std::string_view ini_str = "";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("with old style wildcard for inbound and no public ip, fails")
  {
    std::string_view ini_str = R"(
[bind]
0.0.0.0=443
)";
    REQUIRE_THROWS(run_config_test(env, ini_str));
  }
  SECTION("with old style wildcard for outbound")
  {
    std::string_view ini_str = R"(
[bind]
*=1443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("with wildcard via inbound directive no public ip given, fails")
  {
    std::string_view ini_str = R"(
[bind]
inbound=0.0.0.0:443
)";

    REQUIRE_THROWS(run_config_test(env, ini_str));
  }
  SECTION("with wildcard via inbound directive primary public ip given")
  {
    std::string_view ini_str = R"(
[router]
public-ip=1.1.1.1
public-port=443
[bind]
inbound=0.0.0.0:443
)";

    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("with wildcard via inbound directive secondary public ip given")
  {
    std::string_view ini_str = R"(
[router]
public-ip=2.1.1.1
public-port=443
[bind]
inbound=0.0.0.0:443
)";

    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
  SECTION("with bind via interface name")
  {
    std::string_view ini_str = R"(
[bind]
mock0=443
)";
    REQUIRE_NOTHROW(run_config_test(env, ini_str));
  }
}
