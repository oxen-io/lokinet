#ifndef LLARP_LINK_UTP_HPP
#define LLARP_LINK_UTP_HPP

#include <llarp/link_layer.hpp>

namespace llarp
{
  namespace utp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(llarp::Router* r);
  }
}  // namespace llarp

#endif
