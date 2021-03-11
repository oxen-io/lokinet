#include <util/decaying_hashset.hpp>
#include <router_id.hpp>
#include <catch2/catch.hpp>

TEST_CASE("DecayingHashSet test decay static time", "[decaying-hashset]")
{
  static constexpr auto timeout = 5s;
  static constexpr auto now = 1s;
  llarp::util::DecayingHashSet<llarp::RouterID> hashset{timeout};
  const llarp::RouterID zero{};
  REQUIRE(zero.IsZero());
  REQUIRE(not hashset.Contains(zero));
  REQUIRE(hashset.Insert(zero, now));
  REQUIRE(hashset.Contains(zero));
  hashset.Decay(now + 1s);
  REQUIRE(hashset.Contains(zero));
  hashset.Decay(now + timeout);
  REQUIRE(not hashset.Contains(zero));
  hashset.Decay(now + timeout + 1s);
  REQUIRE(not hashset.Contains(zero));
}

TEST_CASE("DecayingHashSet test decay dynamic time", "[decaying-hashset]")
{
  static constexpr llarp_time_t timeout = 5s;
  const auto now = llarp::time_now_ms();
  llarp::util::DecayingHashSet<llarp::RouterID> hashset{timeout};
  const llarp::RouterID zero{};
  REQUIRE(zero.IsZero());
  REQUIRE(not hashset.Contains(zero));
  REQUIRE(hashset.Insert(zero, now));
  REQUIRE(hashset.Contains(zero));
  hashset.Decay(now + 1s);
  REQUIRE(hashset.Contains(zero));
  hashset.Decay(now + timeout);
  REQUIRE(not hashset.Contains(zero));
  hashset.Decay(now + timeout + 1s);
  REQUIRE(not hashset.Contains(zero));
}
