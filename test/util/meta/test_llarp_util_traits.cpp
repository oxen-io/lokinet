#include <util/meta/traits.hpp>

#include <list>

#include <catch2/catch.hpp>

using namespace llarp;

TEST_CASE("traits::Bottom smoke test")
{
  traits::Bottom bottom;
  (void)bottom;
  SUCCEED();
}

// clang-format off
using ContainerTypes = std::tuple<
    std::tuple< std::vector< int >, std::integral_constant< bool, true > >,
    std::tuple< std::vector< std::string >, std::integral_constant< bool, true > >,
    std::tuple< std::list< std::string >, std::integral_constant< bool, true > >,
    std::tuple< std::string, std::integral_constant< bool, true > >,
    std::tuple< std::shared_ptr<std::string>, std::integral_constant< bool, false > >,
    std::tuple< std::tuple<std::string>, std::integral_constant< bool, false > >,
    std::tuple< int, std::integral_constant< bool, false > >
>;
// clang-format on

TEMPLATE_LIST_TEST_CASE("is_container smoke test", "", ContainerTypes)
{
  bool expected = std::tuple_element_t<1, TestType>::value;
  bool result = traits::is_container<std::tuple_element_t<0, TestType>>::value;
  REQUIRE(expected == result);
}

// clang-format off
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

// clang-format off
using namespace traits::Switch;
using SwitchTypes = std::tuple<
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
// clang-format off

TEMPLATE_LIST_TEST_CASE("Switch smoke test", "", SwitchTypes)
{
  char expected = std::tuple_element_t<0, TestType>::value;
  using InputType = typename std::tuple_element_t<1, TestType>::Type;
  char result = f(InputType());
  REQUIRE(expected == result);
}

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

// clang-format off
using SelectTypes = std::tuple<
  std::tuple<std::integral_constant<char, '0'>, double >,
  std::tuple<std::integral_constant<char, 'b'>, bool >,
  std::tuple<std::integral_constant<char, 'c'>, char >,
  std::tuple<std::integral_constant<char, 's'>, std::string >
>;
// clang-format on

TEMPLATE_LIST_TEST_CASE("selectCase smoke test", "", SelectTypes)
{
  char expected = std::tuple_element_t<0, TestType>::value;
  char result = selectCase<std::tuple_element_t<1, TestType>>();
  REQUIRE(expected == result);
}
// clang-format on

// clang-format off
using PointerTypes = std::tuple<
    std::tuple< int *, std::true_type >,
    std::tuple< int, std::integral_constant< bool, false > >,
    std::tuple< std::shared_ptr<int>, std::true_type >,
    std::tuple< std::unique_ptr<int>, std::true_type >
>;
// clang-format on

TEMPLATE_LIST_TEST_CASE("is_pointy smoke test", "", PointerTypes)
{
  bool expected = std::tuple_element_t<1, TestType>::value;
  bool result = traits::is_pointy<std::tuple_element_t<0, TestType>>::value;
  REQUIRE(expected == result);
}
