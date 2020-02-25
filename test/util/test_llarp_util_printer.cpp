#include <util/printer.hpp>
#include <catch2/catch.hpp>
#include <sstream>
#include <map>

using namespace llarp;

struct PrintableType
{
  std::ostream &
  print(std::ostream &stream, int level, int spaces) const
  {
    stream << "PrintableType " << level << " " << spaces;
    return stream;
  }
};

template <typename T>
std::string print(const T &x) {
  std::ostringstream os;
  {
    Printer printer(os, -1, -1);
    printer.printValue(x);
  }
  return os.str();
}

TEST_CASE("printable types", "[printer]") {
  REQUIRE( print(char('a')) == "[ 'a' ]" );
  REQUIRE( print(bool(true)) == "[ true ]" );
  REQUIRE( print(bool(false)) == "[ false ]" );
  REQUIRE( print(short(123)) == "[ 123 ]" );
  REQUIRE( print(int(std::numeric_limits<int>::max() - 1)) == "[ 2147483646 ]" );
  REQUIRE( print(static_cast< unsigned int >(std::numeric_limits< int >::max()) + 1) == "[ 2147483648 ]" );

  using Catch::Matchers::StartsWith;
  using Catch::Matchers::EndsWith;
  static const char PTR_TYPE[] = "abacus";
  REQUIRE_THAT( print(static_cast< const void * >(PTR_TYPE)), StartsWith("[ 0x") && EndsWith(" ]") );
  REQUIRE( print(static_cast< const char * >(PTR_TYPE)) == R"([ "abacus" ])" );
  REQUIRE( print(std::string("abacus")) == R"([ "abacus" ])" );

  static const int INT_VAL = 100;
  REQUIRE_THAT( print(static_cast< const int * >(&INT_VAL)), StartsWith("[ 0x") && EndsWith(" ]") );
  REQUIRE( print(std::pair< int, std::string >(100, "abacus")) == R"([ [ 100 "abacus" ] ])" );
  REQUIRE( print(std::tuple< int, std::string, int >(100, "abacus", 123)) == R"([ [ 100 "abacus" 123 ] ])" );
  REQUIRE( print(std::map< std::string, char >{{"one", 'a'}, {"two", 'b'}, {"three", 'c'}})
    == R"([ [ [ "one" 'a' ] [ "three" 'c' ] [ "two" 'b' ] ] ])" );
  REQUIRE( print(PrintableType()) == "[ PrintableType -2 -1 ]" );
};


template <typename T>
std::string printAttribute(const std::string& attr, const T &x) {
  std::ostringstream os;
  {
    Printer printer(os, -1, -1);
    printer.printAttribute(attr, x);
  }
  return os.str();
}

TEST_CASE("printable types, with attribute", "[printer]") {
    REQUIRE( printAttribute("fee", char('a')) == "[ fee = 'a' ]" );
    REQUIRE( printAttribute("fi", int(32)) == "[ fi = 32 ]" );
    REQUIRE( printAttribute("fo", std::map< std::string, char >{{"one", 'a'}, {"two", 'b'}, {"three", 'c'}})
        == R"([ fo = [ [ "one" 'a' ] [ "three" 'c' ] [ "two" 'b' ] ] ])" );
    REQUIRE( printAttribute("fum", PrintableType()) == "[ fum = PrintableType -2 -1 ]");
}

void printAnother(Printer &) {}

template <typename T, typename... Tmore>
void printAnother(Printer &p, const std::string &attr, const T& x, const Tmore&... more) {
  p.printAttribute(attr, x);
  printAnother(p, more...);
}

template <typename... T>
std::string printMany(const T&... x) {
  std::ostringstream os;
  {
    Printer p(os, -1, -1);
    printAnother(p, x...);
  }
  return os.str();
}

TEST_CASE("printable types, with multiple attributes", "[printer]") {
  REQUIRE( printMany("val", 1, "v2", 2, "v3", 3, "str", std::string{"xxx"})
      == "[ val = 1 v2 = 2 v3 = 3 str = \"xxx\" ]" );
  REQUIRE( printMany("str", std::string{"xxx"}) == "[ str = \"xxx\" ]" );
}

