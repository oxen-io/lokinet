#ifndef LLARP_DHT_KEY_HPP
#define LLARP_DHT_KEY_HPP

#include <util/aligned.hpp>

#include <array>

namespace llarp
{
  namespace dht
  {
    struct Key_t : public AlignedBuffer< 32 >
    {
      explicit Key_t(const byte_t* buf) : AlignedBuffer< SIZE >(buf)
      {
      }

      explicit Key_t(const Data& val) : AlignedBuffer< SIZE >(val)
      {
      }

      explicit Key_t(const AlignedBuffer< SIZE >& val)
          : AlignedBuffer< SIZE >(val)
      {
      }

      Key_t() : AlignedBuffer< SIZE >()
      {
      }

      /// get snode address string
      std::string
      SNode() const
      {
        const RouterID rid{as_array()};
        return rid.ToString();
      }

      Key_t
      operator^(const Key_t& other) const
      {
        Key_t dist;
        std::transform(begin(), end(), other.begin(), dist.begin(),
                       std::bit_xor< byte_t >());
        return dist;
      }

      bool
      operator==(const Key_t& other) const
      {
        return as_array() == other.as_array();
      }

      bool
      operator!=(const Key_t& other) const
      {
        return as_array() != other.as_array();
      }

      bool
      operator<(const Key_t& other) const
      {
        return as_array() < other.as_array();
      }

      bool
      operator>(const Key_t& other) const
      {
        return as_array() > other.as_array();
      }
    };
  }  // namespace dht
}  // namespace llarp

#endif
