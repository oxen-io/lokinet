#include <util/meta/memfn.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;

struct Foo
{
  bool
  empty()
  {
    return false;
  }

  bool
  constEmpty() const
  {
    return true;
  }

  int
  arg(int v)
  {
    return v + 1;
  }

  int
  constArg(int v) const
  {
    return v - 1;
  }
};

TEST(MemFn, call)
{
  Foo foo;
  ASSERT_FALSE(util::memFn(&Foo::empty, &foo)());
  ASSERT_TRUE(util::memFn(&Foo::constEmpty, &foo)());
  ASSERT_EQ(11, util::memFn(&Foo::arg, &foo)(10));
  ASSERT_EQ(9, util::memFn(&Foo::constArg, &foo)(10));

  ASSERT_TRUE(util::memFn(&Foo::constEmpty, &foo)());
  ASSERT_EQ(9, util::memFn(&Foo::constArg, &foo)(10));
}

template < typename T >
class MemFnType : public ::testing::Test
{
};

TYPED_TEST_SUITE_P(MemFnType);

TYPED_TEST_P(MemFnType, Smoke)
{
  TypeParam foo{};
  ASSERT_TRUE(util::memFn(&Foo::constEmpty, &foo)());
}

REGISTER_TYPED_TEST_SUITE_P(MemFnType, Smoke);

// clang-format off
using MemFnTypes = ::testing::Types<
  Foo, const Foo>;

INSTANTIATE_TYPED_TEST_SUITE_P(MemFn, MemFnType, MemFnTypes);
// clang-format on
