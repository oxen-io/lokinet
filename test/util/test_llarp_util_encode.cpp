#include <gtest/gtest.h>

#include <util/aligned.hpp>
#include <util/encode.hpp>

struct Base32Test : public ::testing::Test
{
  Base32Test()
  {
  }
};

TEST_F(Base32Test, Serialize)
{
  llarp::AlignedBuffer< 32 > addr, otherAddr;
  addr.Randomize();
  char tmp[64]        = {0};
  std::string encoded = llarp::Base32Encode(addr, tmp);
  ASSERT_TRUE(llarp::Base32Decode(tmp, otherAddr));
  ASSERT_TRUE(otherAddr == addr);
}
