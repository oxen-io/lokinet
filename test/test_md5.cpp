#include <gtest/gtest.h>
#include <abyss/md5.hpp>

TEST(TestMD5, TestMD5)
{
  std::string str("The quick brown fox jumps over the lazy dog");
  auto H = MD5::SumHex(str);
  ASSERT_EQ(H, "9e107d9d372bb6826bd81d3542a419d6");
};
