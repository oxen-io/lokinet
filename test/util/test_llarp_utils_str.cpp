#include <util/str.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

struct CmpTestData
{
  bool lt;
  std::string lhs;
  std::string rhs;
};

class CaselessCmpTest : public ::testing::TestWithParam< CmpTestData >
{
};

TEST_P(CaselessCmpTest, test)
{
  CaselessCmp cmp;
  auto d = GetParam();
  ASSERT_EQ(d.lt, cmp(d.lhs, d.rhs));
}

std::vector< CmpTestData > CMPTESTDATA{
    {true, "", "1"},       {false, "1", ""},     {true, "abc", "abcd"},
    {true, "abc", "abd"},  {false, "11", "1"},   {false, "a", "A"},
    {false, "abc", "aBc"}, {false, "ABC", "abc"}};

INSTANTIATE_TEST_SUITE_P(TestStr, CaselessCmpTest, ValuesIn(CMPTESTDATA));

using TestData = std::pair< bool, std::string >;

class TestIsFalseValue : public ::testing::TestWithParam< TestData >
{
};

TEST_P(TestIsFalseValue, test)
{
  ASSERT_EQ(GetParam().first, IsFalseValue(GetParam().second));
}

std::vector< TestData > FALSE_DATA{
    {true, "false"}, {true, "FaLsE"}, {true, "no"},       {true, "nO"},
    {true, "No"},    {true, "NO"},    {true, "NO"},       {true, "0"},
    {true, "off"},   {true, "oFF"},   {false, "false y"}, {false, "true"},
    {false, "tRue"}, {false, "on"}};

INSTANTIATE_TEST_SUITE_P(TestStr, TestIsFalseValue, ValuesIn(FALSE_DATA));

class TestIsTrueValue : public ::testing::TestWithParam< TestData >
{
};

TEST_P(TestIsTrueValue, test)
{
  ASSERT_EQ(GetParam().first, IsTrueValue(GetParam().second));
}

std::vector< TestData > TRUE_DATA{
    {true, "true"},   {true, "TruE"}, {true, "yes"},      {true, "yeS"},
    {true, "yES"},    {true, "YES"},  {true, "1"},        {false, "0"},
    {true, "on"},     {true, "oN"},   {false, "false y"}, {false, "truth"},
    {false, "false"}, {false, "off"}};

INSTANTIATE_TEST_SUITE_P(TestStr, TestIsTrueValue, ValuesIn(TRUE_DATA));
