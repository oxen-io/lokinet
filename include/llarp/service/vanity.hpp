
#ifndef LLARP_SERVICE_VANITY_HPP
#define LLARP_SERVICE_VANITY_HPP
#include <llarp/aligned.hpp>
namespace llarp
{
  namespace service
  {
    /// hidden service address

    using VanityNonce = llarp::AlignedBuffer< 16 >;
  }  // namespace service
}  // namespace llarp
#endif
