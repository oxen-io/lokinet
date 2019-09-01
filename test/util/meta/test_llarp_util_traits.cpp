#include <util/meta/traits.hpp>

#include <list>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;

TEST(traits_bottom, Smoke)
{
  traits::Bottom bottom;
  (void)bottom;
  SUCCEED();
}

template < typename T >
class IsContainer : public ::testing::Test
{
};

TYPED_TEST_SUITE_P(IsContainer);

TYPED_TEST_P(IsContainer, Smoke)
{
  bool expected = std::tuple_element_t< 1, TypeParam >::value;
  bool result =
      traits::is_container< std::tuple_element_t< 0, TypeParam > >::value;
  ASSERT_EQ(expected, result);
}

REGISTER_TYPED_TEST_SUITE_P(IsContainer, Smoke);

// clang-format off
using ContainerTypes = ::testing::Types<
    std::tuple< std::vector< int >, std::integral_constant< bool, true > >,
    std::tuple< std::vector< std::string >, std::integral_constant< bool, true > >,
    std::tuple< std::list< std::string >, std::integral_constant< bool, true > >,
    std::tuple< std::string, std::integral_constant< bool, true > >,
    std::tuple< std::shared_ptr<std::string>, std::integral_constant< bool, false > >,
    std::tuple< std::tuple<std::string>, std::integral_constant< bool, false > >,
    std::tuple< int, std::integral_constant< bool, false > >
>;
INSTANTIATE_TYPED_TEST_SUITE_P(traits, IsContainer, ContainerTypes);

struct A { };
struct B { };
struct C { };
struct D { };
struct E { };
struct F { };
struct G { };
struct H { };
struct I { };
struct J { };

char f(A) { return 'A'; }
char f(B) { return 'B'; }
char f(C) { return 'C'; }
char f(D) { return 'D'; }
char f(E) { return 'E'; }
char f(F) { return 'F'; }
char f(G) { return 'G'; }
char f(H) { return 'H'; }
char f(I) { return 'I'; }
char f(J) { return 'J'; }
char f(traits::Bottom) { return '0'; }

// clang-format on

template < typename T >
class TestSwitch : public ::testing::Test
{
};

TYPED_TEST_SUITE_P(TestSwitch);

TYPED_TEST_P(TestSwitch, Smoke)
{
  char expected   = std::tuple_element_t< 0, TypeParam >::value;
  using InputType = typename std::tuple_element_t< 1, TypeParam >::Type;
  char result     = f(InputType());
  ASSERT_EQ(expected, result);
}

REGISTER_TYPED_TEST_SUITE_P(TestSwitch, Smoke);

// clang-format off
using namespace traits::Switch;
using SwitchTypes = ::testing::Types<
  std::tuple<std::integral_constant<char, 'A'>, Switch< 0, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'B'>, Switch< 1, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'C'>, Switch< 2, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'D'>, Switch< 3, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'E'>, Switch< 4, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'F'>, Switch< 5, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'G'>, Switch< 6, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'H'>, Switch< 7, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'I'>, Switch< 8, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'J'>, Switch< 9, A, B, C, D, E, F, G, H, I, J > >,
  std::tuple<std::integral_constant<char, 'J'>, Switch< 9, C, C, C, C, C, C, C, C, C, J > >,
  std::tuple<std::integral_constant<char, 'C'>, Switch< 6, C, C, C, C, C, C, C, C, C, J > >,
  std::tuple<std::integral_constant<char, '0'>, Switch< 10, A, B, C, D, E, F, G, H, I, J > >
>;

INSTANTIATE_TYPED_TEST_SUITE_P(traits, TestSwitch, SwitchTypes);

template<typename T>
using is_bool = std::is_same<T, bool>;
template<typename T>
using is_char = std::is_same<T, char>;
template<typename T>
using is_string = std::is_same<T, std::string>;

char dispatch(traits::select::Case<>) { return '0'; }
char dispatch(traits::select::Case<is_bool>) { return 'b'; }
char dispatch(traits::select::Case<is_char>) { return 'c'; }
char dispatch(traits::select::Case<is_string>) { return 's'; }

template < typename Type >
char
selectCase()
{
  using Selection = traits::select::Select<Type, is_bool, is_char, is_string >;

  return dispatch(Selection());
}

template < typename T >
class Select : public ::testing::Test
{
};

TYPED_TEST_SUITE_P(Select);

TYPED_TEST_P(Select, Smoke)
{
  char expected   = std::tuple_element_t< 0, TypeParam >::value;
  char result     = selectCase<std::tuple_element_t< 1, TypeParam > >();
  ASSERT_EQ(expected, result);
}

REGISTER_TYPED_TEST_SUITE_P(Select, Smoke);

using SelectTypes = ::testing::Types<
  std::tuple<std::integral_constant<char, '0'>, double >,
  std::tuple<std::integral_constant<char, 'b'>, bool >,
  std::tuple<std::integral_constant<char, 'c'>, char >,
  std::tuple<std::integral_constant<char, 's'>, std::string >
>;

INSTANTIATE_TYPED_TEST_SUITE_P(traits, Select, SelectTypes);

// clang-format on

template < typename T >
class IsPointy : public ::testing::Test
{
};

TYPED_TEST_SUITE_P(IsPointy);

TYPED_TEST_P(IsPointy, Smoke)
{
  bool expected = std::tuple_element_t< 1, TypeParam >::value;
  bool result =
      traits::is_pointy< std::tuple_element_t< 0, TypeParam > >::value;
  ASSERT_EQ(expected, result);
}

REGISTER_TYPED_TEST_SUITE_P(IsPointy, Smoke);

// clang-format off
using PointerTypes = ::testing::Types<
    std::tuple< int *, std::true_type >,
    std::tuple< int, std::integral_constant< bool, false > >,
    std::tuple< std::shared_ptr<int>, std::true_type >,
    std::tuple< std::unique_ptr<int>, std::true_type >
>;
INSTANTIATE_TYPED_TEST_SUITE_P(traits, IsPointy, PointerTypes);
// clang-format on
