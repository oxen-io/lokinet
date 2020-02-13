#include <gtest/gtest.h>
#include <array>
#include <absl/types/variant.h>

#include <util/bits.hpp>

using ArrayUC1  = std::array< unsigned char, 1 >;
using ArrayUC20 = std::array< unsigned char, 20 >;
using ArrayU1   = std::array< unsigned int, 1 >;
using ArrayULL1 = std::array< unsigned long long, 1 >;

using TestType = absl::variant< ArrayUC1, ArrayUC20, ArrayU1, ArrayULL1 >;

struct InputData
{
  TestType data;
  size_t result;
};

struct TestBits : public ::testing::TestWithParam< InputData >
{
};

TEST_P(TestBits, bitcount)
{
  auto d = GetParam();
  ASSERT_EQ(d.result,
            absl::visit(
                [](const auto& v) { return llarp::bits::count_array_bits(v); },
                d.data));
}

// clang-format off
static const InputData inputData[] = {
    {ArrayUC1{{0b00000000}}, 0},
    {ArrayUC1{{0b00000001}}, 1},
    {ArrayUC1{{0b00000010}}, 1},
    {ArrayUC1{{0b00000100}}, 1},
    {ArrayUC1{{0b00001000}}, 1},
    {ArrayUC1{{0b00010000}}, 1},
    {ArrayUC1{{0b00100000}}, 1},
    {ArrayUC1{{0b01000000}}, 1},
    {ArrayUC1{{0b10000000}}, 1},
    {ArrayUC1{{0b11111111}}, 8},
    {ArrayUC20{{0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
               0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
               0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000,
               0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000}}, 0},
    {ArrayUC20{{0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100,
               0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100,
               0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100,
               0b11111111, 0b00000100, 0b00000100, 0b00000100, 0b00000100}}, 48},
    {ArrayU1{{0b00000000000000000000000000000000}}, 0},
    {ArrayU1{{0b00101010101010101010101010101010}}, 15},
    {ArrayU1{{0b10101010101010101010101010101010}}, 16},
    {ArrayU1{{0b01010101010101010101010101010101}}, 16},
    {ArrayU1{{0b11111111111111111111111111111111}}, 32},
    {ArrayULL1{{0b0000000000000000000000000000000000000000000000000000000000000000}}, 0},
    {ArrayULL1{{0b0010101010101010101010101010101000101010101010101010101010101010}}, 30},
    {ArrayULL1{{0b1010101010101010101010101010101010101010101010101010101010101010}}, 32},
    {ArrayULL1{{0b0101010101010101010101010101010101010101010101010101010101010101}}, 32},
    {ArrayULL1{{0b1111111111111111111111111111111111111111111111111111111111111111}}, 64},
};
// clang-format on

INSTANTIATE_TEST_SUITE_P(TestBits, TestBits, ::testing::ValuesIn(inputData));
