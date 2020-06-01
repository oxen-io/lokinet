#include <util/decaying_hashset.hpp>
#include <router_id.hpp>
#include <catch2/catch.hpp>

TEST_CASE("Thrash DecayingHashSet", "[decaying-hashset]")
{
  static constexpr auto duration = 5s;

  static constexpr auto decayInterval = 50ms;
  llarp::util::DecayingHashSet<llarp::AlignedBuffer<32>> hashset(decayInterval);
  const llarp_time_t started = llarp::time_now_ms();
  const auto end = duration + started;
  llarp_time_t nextDecay = started + decayInterval;
  do
  {
    const auto now = llarp::time_now_ms();
    for (size_t i = 0; i < 500; i++)
    {
      llarp::AlignedBuffer<32> rando;
      rando.Randomize();
      hashset.Insert(rando, now);
      /// maybe reinsert to simulate filter hits
      if (i % 20 == 0)
        hashset.Insert(rando, now);
    }
    if (now >= nextDecay)
    {
      REQUIRE(not hashset.Empty());
      hashset.Decay(now);
      nextDecay += decayInterval;
    }

  } while (llarp::time_now_ms() <= end);
}

TEST_CASE("DecayingHashSet test decay static time", "[decaying-hashset]")
{
  static constexpr auto timeout = 5s;
  static constexpr auto now = 1s;
  llarp::util::DecayingHashSet<llarp::RouterID> hashset(timeout);
  const llarp::RouterID zero;
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

TEST_CASE("DecayingHashSet tset decay dynamic time", "[decaying-hashset]")
{
  static constexpr llarp_time_t timeout = 5s;
  const llarp_time_t now = llarp::time_now_ms();
  llarp::util::DecayingHashSet<llarp::RouterID> hashset(timeout);
  const llarp::RouterID zero;
  REQUIRE(zero.IsZero());
  REQUIRE(not hashset.Contains(zero));
  REQUIRE(hashset.Insert(zero));
  REQUIRE(hashset.Contains(zero));
  hashset.Decay(now + 1s);
  REQUIRE(hashset.Contains(zero));
  hashset.Decay(now + timeout);
  REQUIRE(not hashset.Contains(zero));
  hashset.Decay(now + timeout + 1s);
  REQUIRE(not hashset.Contains(zero));
}
