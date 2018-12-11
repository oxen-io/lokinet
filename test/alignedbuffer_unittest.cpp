#include <gtest/gtest.h>
#include <llarp/aligned.hpp>

using Buffer_t = llarp::AlignedBuffer< 32 >;
using Map_t    = std::unordered_map< Buffer_t, int, Buffer_t::Hash >;

struct AlignedBufferTest : public ::testing::Test
{
  AlignedBufferTest()
  : crypto(llarp::Crypto::sodium{})
  {
  }

  llarp::Crypto crypto;
};

TEST_F(AlignedBufferTest, TestHash)
{
  Buffer_t k, other_k;
  k.Randomize();
  other_k.Randomize();
  Map_t m;
  ASSERT_TRUE(m.empty());
  ASSERT_TRUE(m.emplace(k, 1).second);
  ASSERT_TRUE(m.find(k) != m.end());
  ASSERT_TRUE(m[k] == 1);
  ASSERT_FALSE(m.find(other_k) != m.end());
  ASSERT_TRUE(m.size() == 1);
  Buffer_t k_copy = k;
  ASSERT_FALSE(m.emplace(k_copy, 2).second);
  ASSERT_FALSE(m[k_copy] == 2);
  ASSERT_TRUE(m[k_copy] == 1);
};
