#ifndef LLARP_DHT_KEY_HPP
#define LLARP_DHT_KEY_HPP
#include <llarp/aligned.hpp>

namespace llarp
{
  namespace dht
  {
    struct Key_t : public llarp::AlignedBuffer< 32 >
    {
      Key_t(const byte_t* val) : llarp::AlignedBuffer< 32 >(val)
      {
      }

      Key_t() : llarp::AlignedBuffer< 32 >()
      {
      }

      Key_t
      operator^(const Key_t& other) const
      {
        Key_t dist;
        for(size_t idx = 0; idx < 4; ++idx)
          dist.l[idx]  = l[idx] ^ other.l[idx];
        return dist;
      }

      bool
      operator<(const Key_t& other) const
      {
        return memcmp(data_l(), other.data_l(), 32) < 0;
      }
    };
  }
}

#endif