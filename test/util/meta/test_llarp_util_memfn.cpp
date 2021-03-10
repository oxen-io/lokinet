#include <util/meta/memfn.hpp>

#include <catch2/catch.hpp>

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

TEST_CASE("memFn call")
{
  Foo foo;
  REQUIRE_FALSE(util::memFn(&Foo::empty, &foo)());
  REQUIRE(util::memFn(&Foo::constEmpty, &foo)());
  REQUIRE(11 == util::memFn(&Foo::arg, &foo)(10));
  REQUIRE(9 == util::memFn(&Foo::constArg, &foo)(10));

  REQUIRE(util::memFn(&Foo::constEmpty, &foo)());
  REQUIRE(9 == util::memFn(&Foo::constArg, &foo)(10));
}

// clang-format off
using MemFnTypes = std::tuple<
  Foo, const Foo>;
// clang-format on

TEMPLATE_LIST_TEST_CASE("memFn type smoke test", "", MemFnTypes)
{
  TestType foo{};
  REQUIRE(util::memFn(&Foo::constEmpty, &foo)());
}
