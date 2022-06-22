#pragma once
#include "platform.hpp"

namespace llarp::dns
{
  /// a dns platform does silently does nothing, successfully
  class Null_Platform : public I_Platform
  {
   public:
    void
    set_resolver(std::string, llarp::SockAddr, bool) override
    {}
  };
}  // namespace llarp::dns
