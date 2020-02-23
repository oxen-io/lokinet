#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include <functional>

#include "../util/meta/traits.hpp"

namespace llarp
{
  /// 128-bit unsigned integer.  Does *not* support
  /// multiplication/division/modulus.
  struct uint128_t
  {
    uint64_t lower, upper;

   private:
    template < typename BinaryOperator,
               typename = traits::void_t< decltype(BinaryOperator{}(0, 0)) > >
    constexpr uint128_t(const uint128_t& a, const uint128_t& b,
                        BinaryOperator op)
    {
      lower = op(a.lower, b.lower);
      upper = op(a.upper, b.upper);
    }

    template < typename UnaryOperator,
               typename = traits::void_t< decltype(UnaryOperator{}(0)) > >
    constexpr uint128_t(const uint128_t& a, UnaryOperator op)
    {
      lower = op(a.lower);
      upper = op(a.upper);
    }

   public:
    // Initializes with 0s
    constexpr uint128_t() : lower{0}, upper{0}
    {
    }

    // Initializes with least-significant value
    constexpr uint128_t(uint64_t lower) : lower{lower}, upper{0}
    {
    }

    // Initializes with upper and lower values
    constexpr uint128_t(uint64_t upper, uint64_t lower)
        : lower{lower}, upper{upper}
    {
    }

    constexpr uint128_t(const uint128_t&) = default;
    constexpr uint128_t(uint128_t&&)      = default;
    constexpr uint128_t&
    operator=(const uint128_t&) = default;
    constexpr uint128_t&
    operator=(uint128_t&&) = default;

    constexpr uint128_t operator&(const uint128_t& o) const
    {
      return {*this, o, std::bit_and< uint64_t >{}};
    }

    /*
    template <typename T, typename = std::enable_if_t<std::is_integral<T>::value
    && std::is_unsigned<T>::value && sizeof(T) <= sizeof(uint64_t)>> constexpr T
    operator&(T o) const
    {
      return lower & o;
    }
    */

    constexpr uint128_t
    operator|(const uint128_t& o) const
    {
      return {*this, o, std::bit_or< uint64_t >{}};
    }

    constexpr uint128_t
    operator^(const uint128_t& o) const
    {
      return {*this, o, std::bit_xor< uint64_t >{}};
    }

    constexpr uint128_t
    operator~() const
    {
      return {*this, std::bit_not< uint64_t >{}};
    }

    explicit constexpr operator bool() const
    {
      return static_cast< bool >(lower) || static_cast< bool >(upper);
    }

    explicit constexpr operator uint8_t() const
    {
      return static_cast< uint8_t >(lower);
    }
    explicit constexpr operator uint16_t() const
    {
      return static_cast< uint16_t >(lower);
    }
    explicit constexpr operator uint32_t() const
    {
      return static_cast< uint32_t >(lower);
    }
    explicit constexpr operator uint64_t() const
    {
      return lower;
    }

    constexpr bool
    operator==(const uint128_t& b) const
    {
      return lower == b.lower && upper == b.upper;
    }

    constexpr bool
    operator!=(const uint128_t& b) const
    {
      return lower != b.lower || upper != b.upper;
    }

    constexpr bool
    operator<(const uint128_t& b) const
    {
      return upper < b.upper || (upper == b.upper && lower < b.lower);
    }

    constexpr bool
    operator<=(const uint128_t& b) const
    {
      return upper < b.upper || (upper == b.upper && lower <= b.lower);
    }

    constexpr bool
    operator>(const uint128_t& b) const
    {
      return upper > b.upper || (upper == b.upper && lower > b.lower);
    }

    constexpr bool
    operator>=(const uint128_t& b) const
    {
      return upper > b.upper || (upper == b.upper && lower >= b.lower);
    }

    constexpr uint128_t&
    operator++()
    {
      if(++lower == 0)
        ++upper;
      return *this;
    }

    constexpr uint128_t
    operator++(int)
    {
      auto copy = *this;
      ++*this;
      return copy;
    }

    constexpr uint128_t
    operator+(const uint128_t& b) const
    {
      uint128_t sum{upper + b.upper, lower + b.lower};
      if(sum.lower < lower)
        ++sum.upper;
      return sum;
    }

    constexpr uint128_t
    operator-(const uint128_t& b) const
    {
      uint128_t diff{upper - b.upper, lower - b.lower};
      if(diff.lower > lower)
        --diff.upper;
      return diff;
    }

    constexpr uint128_t
    operator<<(uint64_t shift) const
    {
      if(shift == 0)
        return *this;
      else if(shift < 64)
        return {upper << shift | lower >> (64 - shift), lower << shift};
      else if(shift == 64)
        return {lower, 0};
      else if(shift < 128)
        return {lower << (shift - 64), 0};
      else
        return {0, 0};
    }

    constexpr uint128_t
    operator>>(uint64_t shift) const
    {
      if(shift == 0)
        return *this;
      else if(shift < 64)
        return {upper >> shift, lower >> shift | upper << (64 - shift)};
      else if(shift == 64)
        return {0, upper};
      else if(shift < 128)
        return {0, upper >> (shift - 64)};
      else
        return {0, 0};
    }

    constexpr uint128_t&
    operator&=(const uint128_t& o)
    {
      return *this = *this & o;
    }
    constexpr uint128_t&
    operator|=(const uint128_t& o)
    {
      return *this = *this | o;
    }
    constexpr uint128_t&
    operator^=(const uint128_t& o)
    {
      return *this = *this ^ o;
    }
    constexpr uint128_t&
    operator<<=(uint64_t shift)
    {
      return *this = *this << shift;
    }
    constexpr uint128_t&
    operator>>=(uint64_t shift)
    {
      return *this = *this >> shift;
    }
    constexpr uint128_t&
    operator+=(const uint128_t& b)
    {
      return *this = *this + b;
    }

    constexpr uint128_t&
    operator-=(const uint128_t& b)
    {
      return *this = *this - b;
    }
  };

}  // namespace llarp

namespace std
{
  // Hash function for uint128_t
  template <>
  struct hash< llarp::uint128_t >
  {
    size_t
    operator()(const llarp::uint128_t& i) const
    {
      size_t h = std::hash< uint64_t >()(i.lower);
      h ^= std::hash< uint64_t >()(i.upper) + 0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };
}  // namespace std
