#ifndef LLARP_SERVICE_TYPES_HPP
#define LLARP_SERVICE_TYPES_HPP
#include <llarp/aligned.hpp>

namespace llarp
{
  namespace service
  {
    /// hidden service address
    typedef llarp::AlignedBuffer< 32 > Address;

    typedef llarp::AlignedBuffer< 16 > VanityNonce;
  }
}

#endif