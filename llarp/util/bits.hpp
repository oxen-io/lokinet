#pragma once

#include <bitset>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <limits>
#include <llarp/net/uint128.hpp>
#include <llarp/net/net_int.hpp>

namespace llarp
{
  namespace bits
  {
    template <typename Int_t>
    constexpr std::size_t
    count_bits(Int_t i)
    {
      static_assert(std::is_integral<Int_t>::value, "Int_t should be an integer");
      static_assert(std::is_unsigned<Int_t>::value, "Int_t should be unsigned");
      return std::bitset<std::numeric_limits<Int_t>::digits>(i).count();
    }

    constexpr std::size_t
    count_bits_128(const uint128_t& i)
    {
      return count_bits(i.upper) + count_bits(i.lower);
    }

    template <>
    constexpr std::size_t
    count_bits(huint32_t x)
    {
      return count_bits(x.h);
    }

    template <>
    constexpr std::size_t
    count_bits(nuint32_t x)
    {
      return count_bits(x.n);
    }

    template <>
    constexpr std::size_t
    count_bits(huint128_t x)
    {
      return count_bits_128(x.h);
    }

    template <>
    constexpr std::size_t
    count_bits(nuint128_t x)
    {
      return count_bits_128(x.n);
    }

    template <typename InputIt>
    constexpr std::size_t
    count_array_bits_impl(InputIt begin, InputIt end)
    {
      return std::accumulate(
          begin, end, 0, [](auto acc, auto val) { return acc + count_bits(val); });
    }

    template <typename T>
    constexpr std::size_t
    count_array_bits(const T& array)
    {
      return count_array_bits_impl(std::begin(array), std::end(array));
    }
  }  // namespace bits
}  // namespace llarp
