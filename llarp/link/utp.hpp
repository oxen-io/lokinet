#ifndef LLARP_LINK_UTP_HPP
#define LLARP_LINK_UTP_HPP

#include <memory>

namespace llarp
{
  struct ILinkLayer;
  struct Router;

  namespace utp
  {
    std::unique_ptr< ILinkLayer >
    NewServer(llarp::Router* r);
  }
}  // namespace llarp

#endif
