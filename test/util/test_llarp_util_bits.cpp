#include <catch2/catch.hpp>
#include <util/bits.hpp>

using namespace llarp::bits;

TEST_CASE("test bit counting, 8-bit", "[bits]") {
  // Workaround for gcc 5's stdlib; we can drop this crap (and drop all the `T`'s below) once we
  // stop supporting it.
  using T =    std::tuple<unsigned char, size_t>;
  auto x = GENERATE(table<unsigned char, size_t>({
    T{0b00000000, 0},
    T{0b00000001, 1},
    T{0b00000010, 1},
    T{0b00000100, 1},
    T{0b00001000, 1},
    T{0b00010000, 1},
    T{0b00100000, 1},
    T{0b01000000, 1},
    T{0b10000000, 1},
    T{0b11111111, 8},
  }));
  std::array<unsigned char, 1> arr{{std::get<0>(x)}};
  auto expected = std::get<1>(x);
  REQUIRE( count_array_bits(arr) == expected );
}

TEST_CASE("test bit counting, 20 x 8-bit", "[bits]") {
  std::array<unsigned char, 20> x{{
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000}};
  REQUIRE( count_array_bits(x) == 0 );

  x = {{
      0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100,
      0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100,
      0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100,
      0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100}};
  REQUIRE( count_array_bits(x) == 48 );
}

TEST_CASE("test bit counting, unsigned int", "[bits]") {
  using T =    std::tuple<unsigned int, size_t>; // gcc 5 workaround
  auto x = GENERATE(table<unsigned int, size_t>({
    T{0b00000000000000000000000000000000, 0},
    T{0b00101010101010101010101010101010, 15},
    T{0b10101010101010101010101010101010, 16},
    T{0b01010101010101010101010101010101, 16},
    T{0b11111111111111111111111111111111, 32},
  }));

  std::array<unsigned int, 1> arr{{std::get<0>(x)}};
  auto expected = std::get<1>(x);
  REQUIRE( llarp::bits::count_array_bits(arr) == expected );
}

TEST_CASE("test bit counting, unsigned long long", "[bits]") {
  using T =    std::tuple<unsigned long long, size_t>; // gcc 5 workaround
  auto x = GENERATE(table<unsigned long long, size_t>({
    T{0b0000000000000000000000000000000000000000000000000000000000000000, 0},
    T{0b0010101010101010101010101010101000101010101010101010101010101010, 30},
    T{0b1010101010101010101010101010101010101010101010101010101010101010, 32},
    T{0b0101010101010101010101010101010101010101010101010101010101010101, 32},
    T{0b1111111111111111111111111111111111111111111111111111111111111111, 64},
  }));

  std::array<unsigned long long, 1> arr{{std::get<0>(x)}};
  auto expected = std::get<1>(x);
  REQUIRE( llarp::bits::count_array_bits(arr) == expected );
}
