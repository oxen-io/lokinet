#include <util/decaying_hashset.hpp>
#include <router_id.hpp>
#include <gtest/gtest.h>

struct DecayingHashSetTest : public ::testing::Test
{
};

TEST_F(DecayingHashSetTest, TestDecayDeterministc)
{
  static constexpr llarp_time_t timeout = 5;
  static constexpr llarp_time_t now     = 1;
  llarp::util::DecayingHashSet< llarp::RouterID > hashset(timeout);
  const llarp::RouterID zero;
  ASSERT_TRUE(zero.IsZero());
  ASSERT_FALSE(hashset.Contains(zero));
  ASSERT_TRUE(hashset.Insert(zero, now));
  ASSERT_TRUE(hashset.Contains(zero));
  hashset.Decay(now + 1);
  ASSERT_TRUE(hashset.Contains(zero));
  hashset.Decay(now + timeout);
  ASSERT_FALSE(hashset.Contains(zero));
  hashset.Decay(now + timeout + 1);
  ASSERT_FALSE(hashset.Contains(zero));
}

TEST_F(DecayingHashSetTest, TestDecay)
{
  static constexpr llarp_time_t timeout = 5;
  const llarp_time_t now                = llarp::time_now_ms();
  llarp::util::DecayingHashSet< llarp::RouterID > hashset(timeout);
  const llarp::RouterID zero;
  ASSERT_TRUE(zero.IsZero());
  ASSERT_FALSE(hashset.Contains(zero));
  ASSERT_TRUE(hashset.Insert(zero));
  ASSERT_TRUE(hashset.Contains(zero));
  hashset.Decay(now + 1);
  ASSERT_TRUE(hashset.Contains(zero));
  hashset.Decay(now + timeout);
  ASSERT_FALSE(hashset.Contains(zero));
  hashset.Decay(now + timeout + 1);
  ASSERT_FALSE(hashset.Contains(zero));
}
