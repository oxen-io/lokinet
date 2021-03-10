#include <router_version.hpp>
#include "router/router.hpp"

#include <catch2/catch.hpp>

using Catch::Matchers::Equals;

TEST_CASE("Compatibility when protocol equal", "[RouterVersion]")
{
  llarp::RouterVersion v1({0, 1, 2}, 1);
  llarp::RouterVersion v2({0, 1, 2}, 1);

  CHECK(v1.IsCompatableWith(v2));
}

TEST_CASE("Compatibility when protocol unequal", "[RouterVersion]")
{
  llarp::RouterVersion older({0, 1, 2}, 1);
  llarp::RouterVersion newer({0, 1, 2}, 2);

  CHECK_FALSE(older.IsCompatableWith(newer));
  CHECK_FALSE(newer.IsCompatableWith(older));
}

TEST_CASE("Empty compatibility", "[RouterVersion]")
{
  llarp::RouterVersion v1({0, 0, 1}, LLARP_PROTO_VERSION);

  CHECK_FALSE(v1.IsCompatableWith(llarp::emptyRouterVersion));
}

TEST_CASE("IsEmpty", "[RouterVersion]")
{
  llarp::RouterVersion notEmpty({0, 0, 1}, LLARP_PROTO_VERSION);
  CHECK_FALSE(notEmpty.IsEmpty());

  CHECK(llarp::emptyRouterVersion.IsEmpty());
}

TEST_CASE("Clear", "[RouterVersion]")
{
  llarp::RouterVersion version({0, 0, 1}, LLARP_PROTO_VERSION);
  CHECK_FALSE(version.IsEmpty());

  version.Clear();

  CHECK(version.IsEmpty());
}

TEST_CASE("BEncode", "[RouterVersion]")
{
  llarp::RouterVersion v1235({1, 2, 3}, 5);

  std::array<byte_t, 128> tmp{};
  llarp_buffer_t buf(tmp);

  CHECK(v1235.BEncode(&buf));

  std::string s((const char*)buf.begin(), (buf.end() - buf.begin()));
  LogInfo("bencoded: ", buf.begin());

  CHECK_THAT((const char*)buf.begin(), Equals("li5ei1ei2ei3ee"));
}

TEST_CASE("BDecode", "[RouterVersion]")
{
  llarp::RouterVersion version;
  version.Clear();

  const std::string bString("li9ei3ei2ei1ee");
  llarp_buffer_t buf(bString.data(), bString.size());
  CHECK(version.BDecode(&buf));

  llarp::RouterVersion expected({3, 2, 1}, 9);

  CHECK(expected == version);
}

TEST_CASE("Decode long version array", "[RouterVersion]")
{
  llarp::RouterVersion version;
  version.Clear();

  const std::string bString("li9ei3ei2ei1ei2ei3ei4ei5ei6ei7ei8ei9ee");
  llarp_buffer_t buf(bString.data(), bString.size());
  CHECK_FALSE(version.BDecode(&buf));
}
