#include <util/printer.hpp>

#include <absl/types/variant.h>
#include <unordered_map>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

struct PrintableType
{
  std::ostream &
  print(std::ostream &stream, int level, int spaces) const
  {
    stream << "PrintableType " << level << " " << spaces;
    return stream;
  }
};

using SingleVariant =
    absl::variant< char, bool, short, int, unsigned int, const void *,
                   const char *, std::string, const int *,
                   std::pair< int, std::string >,
                   std::tuple< int, std::string, int >,
                   std::map< std::string, char >, PrintableType >;

using SingleType = std::pair< SingleVariant, Matcher< std::string > >;

class SingleValueTest : public ::testing::TestWithParam< SingleType >
{
};

TEST_P(SingleValueTest, value)
{
  SingleType d = GetParam();
  std::ostringstream stream;
  {
    Printer printer(stream, -1, -1);
    absl::visit([&](const auto &x) { printer.printValue(x); }, d.first);
  }
  ASSERT_THAT(stream.str(), d.second);
}

static const char PTR_TYPE[] = "abacus";
static const int INT_VAL     = 100;

// clang-format off
static const SingleType singleType[] = {
    {char('a'), StrEq("[ 'a' ]")},
    {bool(true), StrEq("[ true ]")},
    {bool(false), StrEq("[ false ]")},
    {short(123), StrEq("[ 123 ]")},
    {int(INT_MAX - 1), StrEq("[ 2147483646 ]")},
    {static_cast< unsigned int >(std::numeric_limits< int >::max()) + 1, StrEq("[ 2147483648 ]")},
    {static_cast< const void * >(PTR_TYPE), AllOf(StartsWith("[ 0x"), EndsWith(" ]"))},
    {static_cast< const char * >(PTR_TYPE), StrEq("[ \"abacus\" ]")},
    {std::string("abacus"), StrEq("[ \"abacus\" ]")},
    {static_cast< const int * >(&INT_VAL), AllOf(StartsWith("[ 0x"), EndsWith(" ]"))},
    {std::pair< int, std::string >(100, "abacus"), StrEq("[ [ 100 \"abacus\" ] ]")},
    {std::tuple< int, std::string, int >(100, "abacus", 123), StrEq("[ [ 100 \"abacus\" 123 ] ]")},
    {std::map< std::string, char >{{"one", 'a'}, {"two", 'b'}, {"three", 'c'}}, StrEq("[ [ [ \"one\" \'a\' ] [ \"three\" \'c\' ] [ \"two\" 'b' ] ] ]")},
    {PrintableType(), StrEq("[ PrintableType -2 -1 ]")},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(Printer, SingleValueTest,
                         ::testing::ValuesIn(singleType));

using SingleAttributeType =
    std::tuple< std::string, SingleVariant, Matcher< std::string > >;

class SingleAttributeTest
    : public ::testing::TestWithParam< SingleAttributeType >
{
};

TEST_P(SingleAttributeTest, value)
{
  SingleAttributeType d = GetParam();
  std::ostringstream stream;
  {
    Printer printer(stream, -1, -1);
    absl::visit(
        [&](const auto &x) { printer.printAttribute(std::get< 0 >(d), x); },
        std::get< 1 >(d));
  }
  ASSERT_THAT(stream.str(), std::get< 2 >(d));
}

// clang-format off
static const SingleAttributeType singleAttributeType[] = {
    SingleAttributeType("our_value", char('a'), StrEq("[ our_value = 'a' ]")),
    SingleAttributeType("our_value", bool(true), StrEq("[ our_value = true ]")),
    SingleAttributeType("our_value", bool(false), StrEq("[ our_value = false ]")),
    SingleAttributeType("our_value", short(123), StrEq("[ our_value = 123 ]")),
    SingleAttributeType("our_value", int(INT_MAX - 1), StrEq("[ our_value = 2147483646 ]")),
    SingleAttributeType("our_value", static_cast< unsigned int >(std::numeric_limits< int >::max()) + 1, StrEq("[ our_value = 2147483648 ]")),
    SingleAttributeType("our_value", static_cast< const void * >(PTR_TYPE), AllOf(StartsWith("[ our_value = 0x"), EndsWith(" ]"))),
    SingleAttributeType("our_value", static_cast< const char * >(PTR_TYPE), StrEq("[ our_value = \"abacus\" ]")),
    SingleAttributeType("our_value", std::string("abacus"), StrEq("[ our_value = \"abacus\" ]")),
    SingleAttributeType("our_value", static_cast< const int * >(&INT_VAL), AllOf(StartsWith("[ our_value = 0x"), EndsWith(" ]"))),
    SingleAttributeType("our_value", std::pair< int, std::string >(100, "abacus"), StrEq("[ our_value = [ 100 \"abacus\" ] ]")),
    SingleAttributeType("our_value", std::tuple< int, std::string, int >(100, "abacus", 123), StrEq("[ our_value = [ 100 \"abacus\" 123 ] ]")),
    SingleAttributeType("our_value", std::map< std::string, char >{{"one", 'a'}, {"two", 'b'}, {"three", 'c'}}, StrEq("[ our_value = [ [ \"one\" \'a\' ] [ \"three\" \'c\' ] [ \"two\" 'b' ] ] ]")),
    SingleAttributeType("our_value", PrintableType(), StrEq("[ our_value = PrintableType -2 -1 ]")),
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(Printer, SingleAttributeTest,
                         ::testing::ValuesIn(singleAttributeType));

using ManyAttributes =
    std::pair< std::vector< std::pair< std::string, SingleVariant > >,
               Matcher< std::string > >;

class ManyAttributesTest : public ::testing::TestWithParam< ManyAttributes >
{
};

TEST_P(ManyAttributesTest, value)
{
  ManyAttributes d = GetParam();
  std::ostringstream stream;
  {
    Printer printer(stream, -1, -1);
    std::for_each(d.first.begin(), d.first.end(), [&](const auto &y) {
      std::string n = y.first;
      const auto &v = y.second;
      absl::visit([&](const auto &x) { printer.printAttribute(n, x); }, v);
    });
  }
  ASSERT_THAT(stream.str(), d.second);
}

// clang-format off
static const ManyAttributes manyAttributes[] = {
    {{{"val", 1}, {"v2", 2}, {"v3", 3}, {"str", std::string("xxx")}}, StrEq("[ val = 1 v2 = 2 v3 = 3 str = \"xxx\" ]")},
    {{{"str", std::string("xxx")}}, StrEq("[ str = \"xxx\" ]")}
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(Printer, ManyAttributesTest,
                         ::testing::ValuesIn(manyAttributes));
