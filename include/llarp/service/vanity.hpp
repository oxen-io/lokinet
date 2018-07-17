
#ifndef LLARP_SERVICE_VANITY_HPP
#define LLARP_SERVICE_VANITY_HPP
#include <llarp/aligned.hpp>
namespace llarp
{
  namespace service
  {
    /// hidden service address

    typedef llarp::AlignedBuffer< 16 > VanityNonce;
  }  // namespace service
}  // namespace llarp
#endif