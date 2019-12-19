#include <gtest/gtest.h>

#include <util/aligned.hpp>

#include <iostream>
#include <sstream>
#include <type_traits>
#include <unordered_map>

using TestSizes = ::testing::Types< std::integral_constant< std::size_t, 8 >,
                                    std::integral_constant< std::size_t, 12 >,
                                    std::integral_constant< std::size_t, 16 >,
                                    std::integral_constant< std::size_t, 32 >,
                                    std::integral_constant< std::size_t, 64 >,
                                    std::integral_constant< std::size_t, 77 >,
                                    std::integral_constant< std::size_t, 1024 >,
                                    std::integral_constant< std::size_t, 3333 > >;

template < typename T >
struct AlignedBufferTest : public ::testing::Test
{
};

TYPED_TEST_CASE(AlignedBufferTest, TestSizes, );

TYPED_TEST(AlignedBufferTest, Constructor)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());
  EXPECT_EQ(b.size(), TypeParam::value);
}

TYPED_TEST(AlignedBufferTest, CopyConstructor)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());

  Buffer c = b;
  EXPECT_TRUE(c.IsZero());

  c.Fill(1);
  EXPECT_FALSE(c.IsZero());

  Buffer d = c;
  EXPECT_FALSE(d.IsZero());
}

TYPED_TEST(AlignedBufferTest, AltConstructors)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());
  b.Fill(2);

  Buffer c(b.as_array());
  EXPECT_FALSE(c.IsZero());

  Buffer d(c.data());
  EXPECT_FALSE(d.IsZero());
}

TYPED_TEST(AlignedBufferTest, Assignment)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());

  Buffer c;
  c = b;
  EXPECT_TRUE(c.IsZero());

  c.Fill(1);
  EXPECT_FALSE(c.IsZero());

  Buffer d;
  d = c;
  EXPECT_FALSE(d.IsZero());
}

TYPED_TEST(AlignedBufferTest, StreamOut)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());

  std::stringstream stream;

  stream << b;

  EXPECT_EQ(stream.str(), std::string(TypeParam::value * 2, '0'));

  stream.str("");

  b.Fill(255);
  stream << b;

  EXPECT_EQ(stream.str(), std::string(TypeParam::value * 2, 'f'));
}

TYPED_TEST(AlignedBufferTest, BitwiseNot)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());

  Buffer c = ~b;
  EXPECT_FALSE(c.IsZero());

  for(auto val : c.as_array())
  {
    EXPECT_EQ(255, val);
  }

  Buffer d = ~c;
  EXPECT_TRUE(d.IsZero());
}

TYPED_TEST(AlignedBufferTest, Operators)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());

  Buffer c = b;
  EXPECT_EQ(b, c);
  EXPECT_GE(b, c);
  EXPECT_LE(b, c);
  EXPECT_GE(c, b);
  EXPECT_LE(c, b);

  c.Fill(1);
  EXPECT_NE(b, c);
  EXPECT_LT(b, c);
  EXPECT_GT(c, b);
}

TYPED_TEST(AlignedBufferTest, Xor)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  Buffer c;
  b.Fill(255);
  c.Fill(255);
  EXPECT_FALSE(b.IsZero());
  EXPECT_FALSE(c.IsZero());

  Buffer d = b ^ c;
  // 1 ^ 1 = 0
  EXPECT_TRUE(d.IsZero());
  // Verify unchanged
  EXPECT_FALSE(b.IsZero());
  EXPECT_FALSE(c.IsZero());

  Buffer e, f;
  e.Fill(255);
  Buffer g = e ^ f;
  // 1 ^ 0 = 1
  EXPECT_FALSE(g.IsZero());

  Buffer h, i;
  i.Fill(255);
  Buffer j = h ^ i;
  // 0 ^ 1 = 1
  EXPECT_FALSE(j.IsZero());
}

TYPED_TEST(AlignedBufferTest, XorAssign)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b, c;
  b.Fill(255);
  c.Fill(255);
  EXPECT_FALSE(b.IsZero());
  EXPECT_FALSE(c.IsZero());

  b ^= c;
  EXPECT_TRUE(b.IsZero());
}

TYPED_TEST(AlignedBufferTest, Zero)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;

  Buffer b;
  EXPECT_TRUE(b.IsZero());

  b.Fill(127);
  EXPECT_FALSE(b.IsZero());

  b.Zero();
  EXPECT_TRUE(b.IsZero());
}

TYPED_TEST(AlignedBufferTest, TestHash)
{
  using Buffer = llarp::AlignedBuffer< TypeParam::value >;
  using Map_t  = std::unordered_map< Buffer, int, typename Buffer::Hash >;

  Buffer k, other_k;
  k.Randomize();
  other_k.Randomize();
  Map_t m;
  EXPECT_TRUE(m.empty());
  EXPECT_TRUE(m.emplace(k, 1).second);
  EXPECT_TRUE(m.find(k) != m.end());
  EXPECT_TRUE(m[k] == 1);
  EXPECT_FALSE(m.find(other_k) != m.end());
  EXPECT_TRUE(m.size() == 1);
  Buffer k_copy = k;
  EXPECT_FALSE(m.emplace(k_copy, 2).second);
  EXPECT_FALSE(m[k_copy] == 2);
  EXPECT_TRUE(m[k_copy] == 1);
}
