#include <gtest/gtest.h>

#include <dht/key.hpp>

using namespace llarp;

using Array = std::array< byte_t, dht::Key_t::SIZE >;

struct DHT : public ::testing::TestWithParam< Array >
{
};

TEST_P(DHT, constructor)
{
  auto d = GetParam();

  dht::Key_t a(d);
  dht::Key_t b(d.data());
  dht::Key_t c;

  ASSERT_EQ(a, b);

  if(a.IsZero())
  {
    ASSERT_EQ(a, c);
  }
  else
  {
    ASSERT_NE(a, c);
  }
}

static constexpr Array emptyArray{
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

static constexpr Array fullArray{
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static constexpr Array seqArray{
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
     0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
     0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F}};

static const Array data[] = {emptyArray, fullArray, seqArray};

INSTANTIATE_TEST_SUITE_P(TestDhtKey, DHT, ::testing::ValuesIn(data));

TEST(TestDhtKey, eq)
{
  ASSERT_EQ(dht::Key_t(emptyArray), dht::Key_t(emptyArray));
  ASSERT_EQ(dht::Key_t(fullArray), dht::Key_t(fullArray));
  ASSERT_EQ(dht::Key_t(seqArray), dht::Key_t(seqArray));
}
TEST(TestDhtKey, ne)
{
  ASSERT_NE(dht::Key_t(emptyArray), dht::Key_t(fullArray));
  ASSERT_NE(dht::Key_t(emptyArray), dht::Key_t(seqArray));
  ASSERT_NE(dht::Key_t(fullArray), dht::Key_t(seqArray));
}

TEST(TestDhtKey, lt)
{
  ASSERT_LT(dht::Key_t(emptyArray), dht::Key_t(fullArray));
  ASSERT_LT(dht::Key_t(emptyArray), dht::Key_t(seqArray));
  ASSERT_LT(dht::Key_t(seqArray), dht::Key_t(fullArray));
}

TEST(TestDhtKey, gt)
{
  ASSERT_GT(dht::Key_t(fullArray), dht::Key_t(emptyArray));
  ASSERT_GT(dht::Key_t(seqArray), dht::Key_t(emptyArray));
  ASSERT_GT(dht::Key_t(fullArray), dht::Key_t(seqArray));
}

TEST(TestDhtKey, XOR)
{
  ASSERT_EQ(dht::Key_t(emptyArray),
            dht::Key_t(emptyArray) ^ dht::Key_t(emptyArray));

  ASSERT_EQ(dht::Key_t(seqArray),
            dht::Key_t(emptyArray) ^ dht::Key_t(seqArray));

  ASSERT_EQ(dht::Key_t(fullArray),
            dht::Key_t(emptyArray) ^ dht::Key_t(fullArray));

  ASSERT_EQ(dht::Key_t(emptyArray),
            dht::Key_t(fullArray) ^ dht::Key_t(fullArray));

  ASSERT_EQ(dht::Key_t(emptyArray),
            dht::Key_t(seqArray) ^ dht::Key_t(seqArray));

  Array xorResult;
  std::iota(xorResult.rbegin(), xorResult.rend(), 0xE0);
  ASSERT_EQ(dht::Key_t(xorResult),
            dht::Key_t(seqArray) ^ dht::Key_t(fullArray));
}

TEST(TestDhtKey, TestBucketOperators)
{
  dht::Key_t zero;
  dht::Key_t one;
  dht::Key_t three;

  zero.Zero();
  one.Fill(1);
  three.Fill(3);
  ASSERT_LT(zero, one);
  ASSERT_LT(zero, three);
  ASSERT_FALSE(zero > one);
  ASSERT_FALSE(zero > three);
  ASSERT_NE(zero, three);
  ASSERT_FALSE(zero == three);
  ASSERT_EQ(zero ^ one, one);
  ASSERT_LT(one, three);
  ASSERT_GT(three, one);
  ASSERT_NE(one, three);
  ASSERT_FALSE(one == three);
  ASSERT_EQ(one ^ three, three ^ one);
}
