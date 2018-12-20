#ifndef LLARP_DHT_KEY_HPP
#define LLARP_DHT_KEY_HPP

#include <aligned.hpp>

#include <array>

namespace llarp
{
  namespace dht
  {
    struct Key_t : public llarp::AlignedBuffer< 32 >
    {
      Key_t(const byte_t* buf) : llarp::AlignedBuffer< SIZE >(buf)
      {
      }

      Key_t(const std::array< byte_t, SIZE >& val)
          : llarp::AlignedBuffer< SIZE >(val.data())
      {
      }

      Key_t() : llarp::AlignedBuffer< SIZE >()
      {
      }

      Key_t
      operator^(const Key_t& other) const
      {
        Key_t dist;
        std::transform(as_array().begin(), as_array().end(),
                       other.as_array().begin(), dist.as_array().begin(),
                       std::bit_xor< byte_t >());
        return dist;
      }

      bool
      operator==(const Key_t& other) const
      {
        return memcmp(data(), other.data(), SIZE) == 0;
      }

      bool
      operator!=(const Key_t& other) const
      {
        return memcmp(data(), other.data(), SIZE) != 0;
      }

      bool
      operator<(const Key_t& other) const
      {
        return memcmp(data(), other.data(), SIZE) < 0;
      }

      bool
      operator>(const Key_t& other) const
      {
        return memcmp(data(), other.data(), SIZE) > 0;
      }
    };
  }  // namespace dht
}  // namespace llarp

#endif
