#include <llarp/service/address.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Address", "[Address]")
{
  const std::string snode = "8zfiwpgonsu5zpddpxwdurxyb19x6r96xy4qbikff99jwsziws9y.snode";
  const std::string loki = "7okic5x5do3uh3usttnqz9ek3uuoemdrwzto1hciwim9f947or6y.loki";
  const std::string sub = "lokinet.test";
  const std::string invalid = "7okic5x5do3uh3usttnqz9ek3uuoemdrwzto1hciwim9f947or6y.net";
  llarp::service::Address addr;

  SECTION("Parse bad TLD")
  {
    REQUIRE_FALSE(addr.FromString(snode, ".net"));
    REQUIRE_FALSE(addr.FromString(invalid, ".net"));
  }

  SECTION("Parse bad TLD appened on end")
  {
    const std::string bad = loki + ".net";
    REQUIRE_FALSE(addr.FromString(bad, ".net"));
  }

  SECTION("Parse bad TLD appened on end with subdomain")
  {
    const std::string bad = sub + "." + loki + ".net";
    REQUIRE_FALSE(addr.FromString(bad, ".net"));
  }

  SECTION("Parse SNode not Loki")
  {
    REQUIRE(addr.FromString(snode, ".snode"));
    REQUIRE_FALSE(addr.FromString(snode, ".loki"));
  }

  SECTION("Parse Loki not SNode")
  {
    REQUIRE_FALSE(addr.FromString(loki, ".snode"));
    REQUIRE(addr.FromString(loki, ".loki"));
  }

  SECTION("Parse Loki with subdomain")
  {
    const std::string addr_str = sub + "." + loki;
    REQUIRE(addr.FromString(addr_str, ".loki"));
    REQUIRE(addr.subdomain == sub);
    REQUIRE(addr.ToString() == addr_str);
  };

  SECTION("Parse SNode with subdomain")
  {
    const std::string addr_str = sub + "." + snode;
    REQUIRE(addr.FromString(addr_str, ".snode"));
    REQUIRE(addr.subdomain == sub);
    REQUIRE(addr.ToString(".snode") == addr_str);
  }
}
