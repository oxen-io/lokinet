#ifndef LLARP_BITS_HPP
#define LLARP_BITS_HPP

#include <cstddef>

namespace llarp
{
  namespace bits
  {
    template < typename Int_t >
    constexpr std::size_t
    count_bits(const Int_t& i)
    {
      return i == 0 ? 0
                    : ((i & 0x01) == 0x01) ? 1UL + count_bits(i >> 1)
                                           : count_bits(i >> 1);
    }

    template < typename T >
    constexpr std::size_t
    __count_array_bits(const T& array, std::size_t idx)
    {
      return idx < sizeof(T)
          ? count_bits(array[idx]) + __count_array_bits(array, idx + 1)
          : 0;
    }

    template < typename T >
    constexpr std::size_t
    count_array_bits(const T& array)
    {
      return __count_array_bits(array, 0);
    }
  }  // namespace bits
}  // namespace llarp

#endif
